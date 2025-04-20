//
// Created by zball on 2025/4/19.
//

#ifndef CPPJIEQI_MCTSBOARD_H
#define CPPJIEQI_MCTSBOARD_H

#include <algorithm>
#include <random>
#include <cstdint>
#include <unordered_map>
#include <cstdlib>
#include <cassert>
#include <utility>
#include <string>
#include <iomanip>
#include <vector>
#include "../global/global.h"

namespace board {
    namespace _MCTS {
        using namespace std;
        struct Randomizer{
            mt19937_64 rand;
            Randomizer(const char* seed_start = nullptr, const char* seed_end = nullptr){
                if(seed_end){
                    seed_seq seed(seed_start, seed_end);
                    rand.seed(seed);
                }
            }
            uint32_t sample(uint32_t upper_lim, uint32_t lower_lim = 0) noexcept {
                upper_lim -= lower_lim;
                assert(upper_lim != 0);
                uint64_t quplim = uint64_t(upper_lim) * (rand.max() / uint64_t(upper_lim));
                uint64_t gen = rand();
                while(gen >= quplim) gen = rand();
                return (gen % upper_lim) + lower_lim;
            }
            template<class T> inline void shuffle(T start, T end){
                std::shuffle(start, end, rand);
            }
        };
        using RNG = Singleton<Randomizer>;

        struct Move{
            uint8_t from, to;
        };
        struct MoveDesc{
            int8_t offset, preq, rep, type;
            MoveDesc(int offset, int preq, int rep, int type): offset(offset), preq(preq), rep(rep), type(type){}
            // offset: genmove(pos -> pos + offset)
            // preq: if(preq!=offset) board[pos + preq] = '.' |- genmove(pos -> pos + offset)
            // rep : if(rep) for(i=1; board[pos + i*offset]=='.'; ++i) genmove(pos -> pos + i*offset)
            // type: if(type&1){ for(++i; board[pos + i*offset]=='.'; ++i); } genmove(pos -> pos + i*offset)
            //       if(type&2) pos + offset is restricted |- genmove(pos -> pos + offset)
            //       if(type&4) pos < 128 |- genmove(pos -> pos + offset)
        };
        static constexpr int NORTH = -16, EAST = 1, SOUTH = 16, WEST = -1;
        static constexpr int A9 = 51, I0 = 203, BOARD_SIZE = 256;

        struct XiangqiPieceData{
            static constexpr auto _initial_state =
                    "                "
                    "                "
                    "                "
                    "   defgkgfed    "
                    "   .........    "
                    "   .h.....h.    "
                    "   i.i.i.i.i    "
                    "   .........    "
                    "   .........    "
                    "   I.I.I.I.I    "
                    "   .H.....H.    "
                    "   .........    "
                    "   DEFGKGFED    "
                    "                "
                    "                "
                    "               ";
            static constexpr auto restricted_area =
                    "                "
                    "                "
                    "                "
                    "   defgkgfed    "
                    "   .........    "
                    "   .h.....h.    "
                    "   i.i.i.i.i    "
                    "   .........    "
                    "   .........    "
                    "   I.I.I.I.I    "
                    "   .H.___.H.    "
                    "   ...___...    "
                    "   DEF___FED    "
                    "                "
                    "                "
                    "               ";
            static constexpr char _initial_believe_self[] = "RRNNBBAACCPPPPP";
            static constexpr char _initial_believe_oppo[] = "rrnnbbaaccppppp";
            unsigned char display_name[128][16];
            unsigned char piece_type[128];
            uint64_t zobrist_table[30][256]={};
            static constexpr int dir[4] = {NORTH, EAST, SOUTH, WEST};
            vector<MoveDesc> valid_moves[16];

            XiangqiPieceData():display_name(), piece_type(){
                const pair<unsigned char, string> _uni_pieces[] = {
                        {' ', "  "},
                        {'.', "．"},
                        {'R', "俥"},
                        {'N', "傌"},
                        {'B', "相"},
                        {'A', "仕"},
                        {'K', "帅"},
                        {'P', "兵"},
                        {'C', "炮"},
                        {'D', "暗"},
                        {'E', "暗"},
                        {'F', "暗"},
                        {'G', "暗"},
                        {'H', "暗"},
                        {'I', "暗"},
                        {'U', "不"}, // 我方
                        {'r', "车"},
                        {'n', "马"},
                        {'b', "象"},
                        {'a', "士"},
                        {'k', "将"},
                        {'p', "卒"},
                        {'c', "炮"},
                        {'d', "暗"},
                        {'e', "暗"},
                        {'f', "暗"},
                        {'g', "暗"},
                        {'h', "暗"},
                        {'i', "暗"},
                        {'u', "不"}, // 敌方
                };
                mt19937_64 zobrist_gen(0xf80821f01eefd21dul); // a random seed got from random.org
                for(int i=0;i<30;++i){
                    int index = _uni_pieces[i].first;
                    char piece = _uni_pieces[i].first;
                    piece_type[index] = i;
                    strcpy((char*)display_name[index], _uni_pieces[i].second.c_str());
                    if(i>1){
                        for(int j=0;j<16;++j)
                            zobrist_table[i][j] = zobrist_gen();
                        for(int x=3;x<13;++x)
                            for(int y=3;y<12;++y)
                                zobrist_table[i][x<<4 | y]=zobrist_gen();
                        if(i<15){
                            for(int d = 0; d<4; ++d){
                                int base_offset = dir[d];
                                if(piece == 'A' || piece == 'B' || piece == 'F' || piece == 'G') // 士, 相, 暗象, 暗仕
                                    base_offset += dir[(d+1)&3];
                                int final_offset = base_offset;
                                if(piece == 'B' || piece == 'F' || piece == 'E' || piece == 'N') // 相, 暗象, 暗马, 马
                                    final_offset += base_offset;
                                if(piece == 'E' || piece == 'N'){
                                    valid_moves[i].emplace_back(final_offset + dir[(d-1)&3], base_offset, 0, 0);
                                    valid_moves[i].emplace_back(final_offset + dir[(d+1)&3], base_offset, 0, 0);
                                }else{
                                    MoveDesc md(final_offset, base_offset, 0, 0);
                                    if(piece == 'G' || piece == 'K') md.type = 2; // 暗仕, 帅
                                    if(piece == 'C' || piece == 'H') md.type = 1; // 炮, 暗炮
                                    if(piece == 'C' || piece == 'H' || piece == 'R' || piece == 'D' ) // 炮, 暗炮, 车, 暗车
                                        md.rep = 1;
                                    if(piece == 'P' || piece == 'I'){ // 卒, 暗卒
                                        if(d!=2){
                                            if(d!=0) md.type = 4;
                                            valid_moves[i].push_back(md);
                                        }
                                    }else valid_moves[i].push_back(md);
                                }
                            }
                        }
                    }
                }
            }
            const vector<MoveDesc>& get_moves(unsigned char piece){
                return valid_moves[piece_type[piece]];
            }
            inline uint64_t get_zobrist(unsigned char piece, int index) const {
                return zobrist_table[piece_type[piece]][index];
            }
            inline uint64_t get_zobrist_board(unsigned char piece, int pos) const {
                return zobrist_table[piece_type[piece]][pos];
            }
        };
        using GameData = Singleton<XiangqiPieceData>;

        inline bool is_piece(unsigned char piece){ return piece & 64u; }
        inline bool is_self(unsigned char piece){ return (piece & 96u) == 64u; }
        inline bool is_oppo(unsigned char piece){ return (piece & 96u) == 96u; }
        inline unsigned char to_self(unsigned char piece){ return piece & 95u; }
        inline bool is_dark(unsigned char piece){
            piece = to_self(piece);
            return piece > 'C' && piece < 'J' && piece != 'K';
        }
        inline unsigned char make_turn(unsigned char piece){ return is_piece(piece)? piece^32u : piece; }

        struct alignas(16) BelieveState{
            constexpr static int max_remaining = 15 + 1;
            unsigned char remaining[max_remaining];
            inline unsigned char get() noexcept {
                return remaining[RNG::get()->sample(remaining[0])+1];
            }
            inline void remove(char t) noexcept {
                for(int i=1;i<=remaining[0];++i) if(remaining[i] == t){
                    for(int j = i; j<remaining[0];++j)
                        remaining[j] = remaining[j+1];
                    --remaining[0];
                    break;
                }
            }
            inline uint64_t zobrist(){
                XiangqiPieceData* zobrist_source = GameData::get();
                uint64_t result = 0ull;
                for(int i=1;i<=remaining[0];++i)
                    result ^= zobrist_source->get_zobrist(remaining[i], i);
                return result;
            }
            explicit BelieveState(const char* initial_believe):remaining(){
                remaining[0] = strlen(initial_believe);
                memcpy(remaining+1, initial_believe, remaining[0]);
            }
            explicit BelieveState(const BelieveState& oppo, bool revert){
                // assert(revert == true)
                for(int i=1;i<=oppo.remaining[0];++i)
                    remaining[i] = make_turn(oppo.remaining[i]);
                remaining[0] = oppo.remaining[0];
            }
        };
        struct MCTSBoard {
            using BelieveState = BelieveState;
            char board[BOARD_SIZE];
            BelieveState self_covered, oppo_covered;
            uint64_t board_zobrist, bs_zobrist;
            // board is 10*9, adds padding of 3 to assist border check
            uint64_t compute_board_zobrist(bool revert = false){
                XiangqiPieceData* zobrist_source = GameData::get();
                uint64_t result = 0;
                for(int i=3;i<13;++i)
                    for(int j=3;j<12;++j) {
                        int board_idx = i<<4 | j;
                        if(revert)
                            board[board_idx] = make_turn(board[board_idx]);
                        result ^= zobrist_source->get_zobrist_board(board[board_idx], board_idx);
                    }
                return result;
            }
            uint64_t compute_board_zobrist_const()const{
                const XiangqiPieceData* zobrist_source = GameData::get();
                uint64_t result = 0;
                for(int i=3;i<13;++i)
                    for(int j=3;j<12;++j) {
                        int board_idx = i<<4 | j;
                        result ^= zobrist_source->get_zobrist_board(board[board_idx], board_idx);
                    }
                return result;
            }

            MCTSBoard():self_covered(XiangqiPieceData::_initial_believe_self), oppo_covered(XiangqiPieceData::_initial_believe_oppo){
                memcpy(board, XiangqiPieceData::_initial_state, 256);
                bs_zobrist = self_covered.zobrist() ^ oppo_covered.zobrist();
                board_zobrist = compute_board_zobrist();
            }
            [[nodiscard]] uint64_t zobrist()const{ return board_zobrist ^ bs_zobrist; }

            struct Move_Result{ unsigned char reveal, capture; };
            inline void set_piece(int pos, unsigned char piece){
                auto zobrist_source = Singleton<XiangqiPieceData>::get();
                board_zobrist ^= zobrist_source->get_zobrist_board(board[pos], pos) ^ zobrist_source->get_zobrist_board(piece, pos);
                board[pos]=piece;
            }
            Move_Result move(int from, int to){
                unsigned char reveal = board[from], capture = board[to];
                set_piece(to, reveal);
                set_piece(from, '.');
                if(!is_dark(reveal)) reveal = 0u;
                if(capture == '.') capture = 0u;
                return (Move_Result){reveal, capture};
            }
            void reveal(int pos, unsigned char piece){
                set_piece(pos, piece);
                if(is_self(piece)) self_covered.remove(piece); else oppo_covered.remove(piece);
            }
            void undo_move(int to, int from, Move_Result mr){
                unsigned char piece = board[to];
                set_piece(to, mr.capture ? mr.capture : '.');
                set_piece(from, mr.reveal ? mr.reveal : piece);
            }

            // 放在 MCTSBoard 类的 public 区域
//--------------------------------------------------------------------
            void print_raw_board(std::ostream& os = std::cout,
                                 bool show_unicode = false) const
            {
                const auto *data = GameData::get();
                int row = 9;
                for (int idx = A9; idx <= I0; idx += 16, --row)
                {
                    os << ' ' << row << ' ';
                    for (int j = 0; j < 9; ++j)
                    {
                        unsigned char p = board[idx + j];
                        if(is_self(p)){
                            os << "\033[31m";
                        }
                        if (show_unicode)
                            os << std::setw(2) << data->display_name[(uint8_t)p];
                        else
                            os << static_cast<char>(p ? p : '.');
                        os << "\033[0m";
                    }
                    os << "\n";
                }
                os << "   a b c d e f g h i\n\n";
            }


            void print_believe_state(std::ostream& os = std::cout) const
            {
                auto pretty = [](unsigned char c) -> std::string {
                    if (std::isupper(c)) return std::string(1, c);
                    return std::string(1, (char)std::toupper(c));
                };
                auto dump_one = [&](const BelieveState& bs, const char* label)
                {
                    std::unordered_map<char,int> cnt;
                    for (int i = 1; i <= bs.remaining[0]; ++i) ++cnt[bs.remaining[i]];
                    os << label << " (remaining=" << (int)bs.remaining[0] << "): ";
                    for (auto& kv : cnt)
                        os << pretty(kv.first) << '*' << kv.second << ' ';
                    os << '\n';
                };
                dump_one(self_covered,  "SELF ");
                dump_one(oppo_covered,  "OPP  ");
            }

            void dump(std::ostream& os = std::cout, bool unicode = false) const
            {
                os << "===== CURRENT BOARD =====\n";
                print_raw_board(os, unicode);
                os << "===== BELIEVE STATES =====\n";
                print_believe_state(os);
                os << "Zobrist = 0x" << std::hex << zobrist() << std::dec << "\n";
            }
            using Move = _MCTS::Move;
            // move gen
            Move moves[120];
            bool mate;

            void print_moves(const Move* moves, int nmoves, std::ostream& os = std::cout,
                                bool show_unicode = true) const{
                int color[256];
                memset(color, 0, sizeof color);
                const int red = 31;
                const int yellow = 33;
                const int cyan = 36;
                auto switch_color = [&](int color = 0){
                    os << "\033[" << color << "m";
                };
                for(int i=0;i<nmoves;++i) {
                    color[moves[i].from] = yellow;
                    color[moves[i].to] = cyan;
                }
                const auto *data = GameData::get();
                int row = 9;
                for (int idx = A9; idx <= I0; idx += 16, --row)
                {
                    os << ' ' << row << ' ';
                    for (int j = 0; j < 9; ++j)
                    {
                        unsigned char p = board[idx + j];
                        switch_color(color[idx + j]? color[idx + j]: (is_self(p)? red : 0));
                        if (show_unicode) {
                            if(color[idx + j] && p == '.'){
                                os << "[]";
                            }else os << std::setw(2) << data->display_name[(uint8_t) p];
                        }
                        else
                            os << static_cast<char>(p ? p : '.');
                        switch_color();
                    }
                    os << "\n";
                }
                os << "   a b c d e f g h i\n\n";
            }
            int generate_valid_moves(){
                auto database = Singleton<XiangqiPieceData>::get();
                int count = 0;
                mate = false;
                auto genmove=[&](int from, int to){
                    if(board[to]!=' ' && !is_self(board[to])) {
                        moves[count++] = (Move) {(uint8_t) from, (uint8_t) to};
                        if(board[to] == 'k') mate = true;
                    }
                };
                for(int x=3;x<13;++x)
                    for(int y=3;y<12;++y){
                        int pos = x<<4 | y;
                        unsigned char piece = board[pos];
                        if(is_self(piece)){
                            auto moves = database->get_moves(piece);
                            for(const auto&q:moves){
                                if(q.preq != q.offset) {
                                    if (!is_piece(board[pos + q.preq]))
                                        genmove(pos, pos + q.offset);
                                }else{
                                    int off = q.offset;
                                    int topos = pos + off;
                                    if(q.rep){
                                        for(;board[topos]=='.';topos+=off) genmove(pos, topos);
                                        if(q.type == 1){ // cannon
                                            for(topos+=off;board[topos]=='.';topos+=off);
                                            genmove(pos, topos);
                                        }else genmove(pos, topos);
                                    }else if(q.type == 2){
                                        if(XiangqiPieceData::restricted_area[topos] == '_') genmove(pos, topos);
                                    }else if(q.type == 4){
                                        if(topos < 128) genmove(pos, topos); // on opponent side
                                    }else genmove(pos, topos);
                                }
                            }
                        }
                    }
                return count;
            }
            int mate_level(int pos){
                // After making one move, use mate_level to check the mate level of this move
                // can be used to judge the outcome of a repetition
                // 2 => check mate
                // 1 => check capture
                // 0 => nothing
                auto database = Singleton<XiangqiPieceData>::get();
                int level = 0;
                unsigned char piece = board[pos];
                auto check = [&](int pos)->int{ if(is_oppo(board[pos])){ level = 1; if(board[pos]=='k') return 1; } return 0;};
                if(is_self(piece)){
                    auto moves = database->get_moves(piece);
                    for(const auto&q:moves){
                        if(q.preq != q.offset) {
                            if (!is_piece(board[pos + q.preq]))
                                if(check(pos + q.offset)) return 2;
                        }else{
                            int off = q.offset;
                            int topos = pos + off;
                            if(q.rep){
                                for(;board[topos]=='.';topos+=off) if(check(topos)) return 2;
                                if(q.type == 1){ // cannon
                                    for(topos+=off;board[topos]=='.';topos+=off);
                                    if(check(topos)) return 2;
                                }else if(check(topos)) return 2;
                            }else if(q.type == 2){
                                if(XiangqiPieceData::restricted_area[topos] == '_') if(check(topos)) return 2;
                            }else if(q.type == 4){
                                if(topos < 128) if(check(topos)) return 2; // on opponent side
                            }else if(check(topos)) return 2;
                        }
                    }
                }
                return level;
            }
            MCTSBoard(const MCTSBoard& oppo, bool revert): self_covered(oppo.oppo_covered, true), oppo_covered(oppo.self_covered, true){
                // assert(revert == true)
                reverse_copy(oppo.board, oppo.board+255, board);
                bs_zobrist = self_covered.zobrist() ^ oppo_covered.zobrist();
                board_zobrist = compute_board_zobrist(true);
            }
        };

    }
    using MCTSBoard = _MCTS::MCTSBoard;
}


#endif //CPPJIEQI_MCTSBOARD_H
