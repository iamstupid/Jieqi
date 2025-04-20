//
// Created by zball on 2025/4/21.
//

#include "board/DUCT.h"

int main(){
    board::_MCTS::MCTSBoard board;
    MCTSSim::DeterminizedSimulator sim(board, true);
    board::_DUCT::DUCT tree(&sim);
    tree.determinize(1000);
    return 0;
}