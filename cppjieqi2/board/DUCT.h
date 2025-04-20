//
// Created by zball on 2025/4/20.
//

#ifndef CPPJIEQI_DUCT_H
#define CPPJIEQI_DUCT_H

#include <vector>
#include <cmath>
#include <utility>
#include <cassert>
#include <limits>
#include "MCTSBoard.h"
#include "MCTSGameSimulator.h"
#include <malloc.h>

namespace board{
    namespace _DUCT{
        using namespace std;
        using Move = MCTSBoard :: Move;
        struct stats{
            double total_eval = 0, avg_eval = 0;
            int count = 0;
            operator double(){ return avg_eval; }
            stats& operator+=(stats b){
                total_eval += b.total_eval; count += b.count; avg_eval = count ? total_eval/count: 0;
                return *this;
            }
            stats& operator+=(double b){
                total_eval += b; count ++; avg_eval = total_eval / count;
                return *this;
            }
        };
        struct Node{
            uint64_t zobrist = 0ull;
            stats eval;
            Move move;
            Node *child = nullptr, *sibling = nullptr;
            Node* select_child(){
                static constexpr double c = 1.1; // an empirically decided value
                if(!eval.count) return child;
                double rln = c * sqrt(log(eval.count));
                double polMax = -numeric_limits<double>::infinity();
                Node *sel = nullptr, *ptr = child;
                while(ptr){
                    if(!ptr->eval.count) return ptr;
                    double u = ptr->eval.avg_eval + rln / sqrt(eval.count);
                    if(u>polMax){
                        polMax = u;
                        sel = ptr;
                    }
                    ptr = ptr->sibling;
                }
                return sel;
            }
        };

        struct DUCT{
            using game = MCTSSim::DeterminizedSimulator;
            vector<Node*> mem;
            int nallocCount = 0;
            Node* alloc(){
                Node* result = nullptr;
                if(nallocCount < mem.size() * 2048){
                    result = mem[nallocCount >> 11]+(nallocCount & 2047);
                }else{
                    mem.push_back((Node*)calloc(2048, sizeof(Node)));
                    result = mem[nallocCount >> 11];
                }
                ++nallocCount;
                return result;
            }
            ~DUCT(){
                for(auto t: mem) free(t);
            }
            void clearAlloc(){ nallocCount = 0; }
            vector<std::pair<Move, stats> > root_moves;
            vector<Move> moves;
            game* sim;
            uint64_t zobrist;
            DUCT(game* sim):sim(sim){
                moves = sim->generate_moves();
                for(auto move: moves)
                    root_moves.emplace_back(move, stats());
                zobrist = sim->ply1.board_zobrist;
            }
            Node* generate_move_nodes(Node* parent, const vector<Move>& _moves){
                Node* last = nullptr;
                for(auto t = _moves.rbegin(); t!=moves.rend(); ++t){
                    Node* newnode = new(alloc()) Node();
                    newnode->move = *t;
                    newnode->sibling = last;
                    last = newnode;
                }
                return parent->child = last;
            }
            std::string ptos_(int pos){
                int x = 12 - (pos >> 4);
                int y = (pos & 15) - 3;
                char str[3] = {char(x+'0'), char(y+'a'), '\0'};
                return string(str);
            }
            double rollout(int dep_lim = 200, bool verbose = false){
                if(verbose) {
                    std::cout << "移动: " << ptos_(sim->mvstack.top().from) << ptos_(sim->mvstack.top().to)
                              << std::endl;
                    sim->print(std::cout);
                    std::cout << std::endl;
                }
                if(dep_lim <= 0) return 0; // didn't terminate
                auto moves = sim->generate_moves();
                int sel = _MCTS::RNG::get()->sample(moves.size());
                int vali = (double)sim->move(moves[sel].from, moves[sel].to);
                double val = vali;
                if(vali == game::unterminated)
                    val = -rollout(dep_lim - 1, verbose);
                else{
                    if(verbose) {
                        sim->print(std::cout);
                        std::cout << "局面: " << val << "turn:" << (sim->turn?"红":"黑") << '\n';
                    }
                    //
                    if(sim->turn) val = -val; // if this node is on the opponent side, then revert the value
                }
                sim->undo_move();
                if(verbose) {
                    sim->print(std::cout);
                    std::cout << "回退: " << ptos_(sim->mvstack.top().from) << ptos_(sim->mvstack.top().to)
                              << std::endl;
                }
                return val;
            }
            double expand(Node* node, bool verbose = false){
                if(!sim->state_value.empty() && sim->state_value.top() != game::unterminated) // game is terminated
                    return sim->state_value.top();
                if(!node->child){
                    node->zobrist = sim->ply1.board_zobrist;
                    auto _moves = sim->generate_moves();
                    generate_move_nodes(node, _moves);
                }
                if(node->zobrist != sim->ply1.board_zobrist){
                    std::cout << node->zobrist << " : " << sim->ply1.compute_board_zobrist_const() << " : " << sim->ply1.board_zobrist << endl;
                    assert(node->zobrist == sim->ply1.board_zobrist);
                }
                Node* sel = node->select_child();
                sim->move(sel->move.from, sel->move.to);
                double val = 0.;
                if(!sel->eval.count) {
                    val = rollout(200, verbose);
                    sel->eval += val;
                    val = -val;
                }
                else val = -expand(sel, verbose);
                node->eval += val;
                sim->undo_move();
                return val;
            }
            Node* root;
            void determinize(int rollouts){
                sim->determinize();
                // initialize root node
                clearAlloc();
                root = new(alloc()) Node();
                root->zobrist = sim->ply1.board_zobrist;
                // expand root node
                generate_move_nodes(root, moves);
                for(int i=0;i<rollouts;++i){
                    /*
                    sim->print();
                    std::cout << "zobrist: " << sim->ply1.board_zobrist << std::endl;
                     */
                    double val = expand(root, (i & 10) == 9);
                    /*
                    sim->print();
                    std::cout << "zobrist: " << sim->ply1.board_zobrist << std::endl;
                     */
                    // std::cout << "于 MCTS 的根处: " << val << '\n';
                    // std::cout << root -> zobrist << " : " << reinterpret_cast<uint64_t>(root) << " : " << reinterpret_cast<uint64_t>(root->child) << endl;
                }
                Node* ch = root -> child;
                for(auto& stat: root_moves){
                    stat.second += ch->eval;
                    ch = ch->sibling;
                }
            }
        };
    }
}

#endif //CPPJIEQI_DUCT_H
