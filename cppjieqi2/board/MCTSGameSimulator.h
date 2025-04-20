//
// Created by zball on 2025/4/20.
//

#ifndef CPPJIEQI_MCTSGAMESIMULATOR_H
#define CPPJIEQI_MCTSGAMESIMULATOR_H

#include <stack>
#include <algorithm>
#include <unordered_map>
#include "MCTSBoard.h"

namespace MCTSSim{
    using namespace board::_MCTS;
    using std::stack;
    struct DeterminizedSimulator{
        MCTSBoard ply1, ply2;
        BelieveState ply1_piece, ply2_piece;

        unordered_map<uint64_t, int> state_counter;

        char det[256];
        bool turn;
        struct BSObject{
            BelieveState ply1_self, ply1_oppo;
            BelieveState ply2_self, ply2_oppo;
        };
        stack<BSObject> bsstack;

        struct MoveResult{
            uint8_t from, to;
            uint8_t reveal, capture;
            uint8_t mate_level, ncap_count;
        };
        stack<MoveResult> mvstack;
        static constexpr int unterminated = 114514;
        stack<int> state_value;
        int value(int last_pos, const MoveResult&last){
            auto zob = ply1.board_zobrist;
            int t = 1;
            if(state_counter.count(zob)) t=++state_counter[zob];
            else state_counter[zob]=1;
            if(last.capture == 'k') return 1;
            if(last.capture == 'K') return -1;
            if(last.ncap_count >= 120) return 0; // draw
            if(t>=3){
                // rep
                int q = turn ? -1 : 1;
                if(!mvstack.empty()){
                    auto ll = mvstack.top();
                    if(ll.mate_level > last.mate_level) return q;
                    if(ll.mate_level < last.mate_level) return -q;
                    return 0;
                }
            }
            return unterminated;
        }
        vector<MCTSBoard::Move> generate_moves(){
            if(turn){
                int nmoves = ply1.generate_valid_moves();
                if(ply1.mate){
                    for(int i=0;i<nmoves;++i)
                        if(ply1.board[ply1.moves[i].to] == 'k')
                            return {ply1.moves+i, ply1.moves+i+1};
                }else return {ply1.moves, ply1.moves+nmoves};
            }else{
                vector<MCTSBoard::Move> res;
                int nmoves = ply2.generate_valid_moves();
                // std::cout << std::endl;
                if(ply2.mate){
                    for(int i=0;i<nmoves;++i)
                        if(ply2.board[ply2.moves[i].to] == 'k')
                            res = {ply2.moves+i, ply2.moves+i+1};
                }else res = {ply2.moves, ply2.moves+nmoves};
                for(auto& move: res) {
                    move.to = 254 - move.to;
                    move.from = 254 - move.from;
                }
                return res;
            }
        }
        int move(int from, int to){
            auto res = ply1.move(from, to);
            auto res2 = ply2.move(254-from, 254-to);
            if(res.reveal || is_dark(res.capture)) {
                bsstack.push((BSObject){ply1.self_covered, ply1.oppo_covered, ply2.self_covered, ply2.oppo_covered});
                if (res.reveal) {
                    ply1.reveal(to, det[from]);
                    ply2.reveal(254-to, det[from]^32);
                }
                if(is_dark(res.capture)){
                    if(turn) ply1.oppo_covered.remove(det[to]);
                    else ply2.oppo_covered.remove(det[to]^32);
                }
            }
            uint8_t mate_level;
            if(turn){
                mate_level = ply1.mate_level(to);
            }else{
                mate_level = ply2.mate_level(254-to);
            }
            uint8_t capCount = mvstack.empty()? 1 : mvstack.top().ncap_count + 1;
            if(res.capture) capCount = 0;
            auto mr = (MoveResult){(uint8_t)from, (uint8_t)to, res.reveal, res.capture,mate_level,capCount};
            state_value.push(value(to, mr));
            mvstack.push(mr);
            turn = !turn;
            return state_value.top();
        }
        void undo_move(){
            auto t = mvstack.top();
            mvstack.pop();
            state_value.pop();
            state_counter[ply1.board_zobrist]--;
            MCTSBoard::Move_Result mr{t.reveal, t.capture};
            ply1.undo_move(t.to, t.from, mr);
            if(mr.reveal) mr.reveal^=32;
            if(mr.capture) mr.capture^=32;
            ply2.undo_move(254-t.to, 254-t.from, mr);
            if(t.reveal || is_dark(t.capture)){
                auto u = bsstack.top();
                bsstack.pop();
                ply1.self_covered = u.ply1_self;
                ply1.oppo_covered = u.ply1_oppo;
                ply2.self_covered = u.ply2_self;
                ply2.oppo_covered = u.ply2_oppo;
            }
            turn = !turn;
        }

        void determinize(){
            RNG::get()->shuffle(ply1_piece.remaining+1, ply1_piece.remaining+1+ply1_piece.remaining[0]);
            RNG::get()->shuffle(ply2_piece.remaining+1, ply2_piece.remaining+1+ply2_piece.remaining[0]);
            int ply1idx = 1, ply2idx = 1;
            for(int x=3;x<13;++x)
                for(int y=3;y<12;++y){
                    int pos = x<<4 | y;
                    char piece = ply1.board[pos];
                    if(is_dark(piece)) {
                        if (is_self(piece)) det[pos] = ply1_piece.remaining[ply1idx++];
                        if (is_oppo(piece)) det[pos] = ply2_piece.remaining[ply2idx++];
                    }
                }
        }
        void set_det(int pos, char revealed){
            det[pos] = revealed;
        }
        void print(std::ostream& os = std::cout){
            MCTSBoard::Move lastMove[1] = {};
            if(!mvstack.empty()){
                lastMove[0].from = mvstack.top().from;
                lastMove[0].to = mvstack.top().to;
                ply1.print_moves(lastMove, 1, os, false);
            }else{
                ply1.print_moves(lastMove, 0, os, false);
            }
        }
        void print_det(std::ostream& os = std::cout){
            const int red = 31;
            const int yellow = 33;
            const int cyan = 36;
            auto switch_color = [&](int color = 0){
                os << "\033[" << color << "m";
            };
            const auto *data = GameData::get();
            int row = 9;
            for (int idx = A9; idx <= I0; idx += 16, --row)
            {
                os << ' ' << row << ' ';
                for (int j = 0; j < 9; ++j)
                {
                    unsigned char p = det[idx + j];
                    switch_color(is_self(p)? red : 0);
                    os << std::setw(2) << data->display_name[(uint8_t) p];
                    switch_color();
                }
                os << "\n";
            }
            os << "   a b c d e f g h i\n\n";
        }
        DeterminizedSimulator(const MCTSBoard& board, bool turn):ply1(board), ply2(board, true), ply1_piece(board.self_covered),
                                                      ply2_piece(board.oppo_covered),det(), turn(turn){
            memset(det, '.', sizeof det);
        }
    };
};


#endif //CPPJIEQI_MCTSGAMESIMULATOR_H
