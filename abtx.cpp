#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>
#include <climits>
#include <cstring>  // 添加cstring头文件用于memcpy
#include <chrono>  // 添加计时功能

using namespace std;

int currBotColor; // 我所执子颜色（1为黑，-1为白，棋盘状态亦同）
int gridInfo[7][7] = { 0 }; // 先x后y，记录棋盘状态
int blackPieceCount = 2, whitePieceCount = 2;
int startX, startY, resultX, resultY;
static int delta[24][2] = { { 1,1 },{ 0,1 },{ -1,1 },{ -1,0 },
{ -1,-1 },{ 0,-1 },{ 1,-1 },{ 1,0 },
{ 2,0 },{ 2,1 },{ 2,2 },{ 1,2 },
{ 0,2 },{ -1,2 },{ -2,2 },{ -2,1 },
{ -2,0 },{ -2,-1 },{ -2,-2 },{ -1,-2 },
{ 0,-2 },{ 1,-2 },{ 2,-2 },{ 2,-1 } };

// 使用位操作表示棋盘状态
const long long maxBoard = (1LL << 49) - 1;
long long bitBoard[2] = {0, 0}; // 0表示白色，1表示黑色
long long funcID[7][7]; // 每个位置对应的位
long long around1[7][7], aroundPoint1[7][7][8]; // 周围8个位置
long long around2[7][7], aroundPoint2[7][7][16]; // 周围16个位置(跳跃)

// 位操作的方向数组
int dxNumber1[8] = {1, 1, 1, 0, -1, -1, -1, 0};
int dyNumber1[8] = {-1, 0, 1, 1, 1, 0, -1, -1};
int dxNumber2[16] = {2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1, 0, 1};
int dyNumber2[16] = {-2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2};

// 游戏状态结构体
struct GameState {
    int grid[7][7];
    int blackCount;
    int whiteCount;
    
    GameState() {
        memcpy(grid, gridInfo, sizeof(grid));
        blackCount = blackPieceCount;
        whiteCount = whitePieceCount;
    }
};

// 位表示的游戏状态
struct BitGameState {
    long long chess[2]; // 0表示白色，1表示黑色
    int currColor; // 当前颜色，1为黑，-1为白
    
    BitGameState() {
        chess[0] = bitBoard[0];
        chess[1] = bitBoard[1];
        currColor = currBotColor;
    }
};

// 移动结构体
struct Move {
    int startX, startY;
    int endX, endY;
    int score;  // 用于移动排序
};

// 添加计时控制
using Clock = std::chrono::steady_clock;
const double TIME_LIMIT = 0.95; // 留50ms缓冲
Clock::time_point startTime;

// 初始化位操作相关数据
void initBit() {
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            funcID[i][j] = 1LL << (7 * i + j);
        }
    }

    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            // 初始化周围8个方向
            for (int k = 0; k < 8; k++) {
                int nx = i + dxNumber1[k];
                int ny = j + dyNumber1[k];
                if (nx < 0 || nx >= 7 || ny < 0 || ny >= 7)
                    continue;
                aroundPoint1[i][j][k] = funcID[nx][ny];
                around1[i][j] |= aroundPoint1[i][j][k];
            }
            
            // 初始化周围16个跳跃方向
            for (int k = 0; k < 16; k++) {
                int nx = i + dxNumber2[k];
                int ny = j + dyNumber2[k];
                if (nx < 0 || nx >= 7 || ny < 0 || ny >= 7)
                    continue;
                aroundPoint2[i][j][k] = funcID[nx][ny];
                around2[i][j] |= aroundPoint2[i][j][k];
            }
        }
    }
}

// 同步网格信息到位棋盘
void syncGridToBitBoard() {
    bitBoard[0] = bitBoard[1] = 0;
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            if (gridInfo[i][j] == 1) {
                bitBoard[1] |= funcID[i][j];
            } else if (gridInfo[i][j] == -1) {
                bitBoard[0] |= funcID[i][j];
            }
        }
    }
}

// 同步位棋盘到网格信息
void syncBitBoardToGrid() {
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            if (bitBoard[1] & funcID[i][j]) {
                gridInfo[i][j] = 1;
            } else if (bitBoard[0] & funcID[i][j]) {
                gridInfo[i][j] = -1;
            } else {
                gridInfo[i][j] = 0;
            }
        }
    }
    
    // 更新棋子数量
    blackPieceCount = __builtin_popcountll(bitBoard[1]);
    whitePieceCount = __builtin_popcountll(bitBoard[0]);
}

// 计算棋子数量（使用位操作）
inline int countBits(long long board) {
    return __builtin_popcountll(board);
}

// 检查是否超时
bool checkTimeout() {
    auto currentTime = Clock::now();
    double elapsed = std::chrono::duration<double>(currentTime - startTime).count();
    return elapsed >= TIME_LIMIT;
}

// 判断是否在地图内
inline bool inMap(int x, int y) {
    if (x < 0 || x > 6 || y < 0 || y > 6)
        return false;
    return true;
}

// 向Direction方向改动坐标，并返回是否越界
inline bool MoveStep(int &x, int &y, int Direction) {
    x = x + delta[Direction][0];
    y = y + delta[Direction][1];
    return inMap(x, y);
}

// 在坐标处落子，检查是否合法或模拟落子
bool ProcStep(int x0, int y0, int x1, int y1, int color) {
    if (color == 0)
        return false;
    if (x1 == -1) // 无路可走，跳过此回合
        return true;
    if (!inMap(x0, y0) || !inMap(x1, y1)) // 超出边界
        return false;
    if (gridInfo[x0][y0] != color)
        return false;
    int dx, dy, x, y, currCount = 0, dir;
    int effectivePoints[8][2];
    dx = abs((x0 - x1)), dy = abs((y0 - y1));
    if ((dx == 0 && dy == 0) || dx > 2 || dy > 2) // 保证不会移动到原来位置，而且移动始终在5×5区域内
        return false;
    if (gridInfo[x1][y1] != 0) // 保证移动到的位置为空
        return false;
    if (dx == 2 || dy == 2) // 如果走的是5×5的外围，则不是复制粘贴
        gridInfo[x0][y0] = 0;
    else
    {
        if (color == 1)
            blackPieceCount++;
        else
            whitePieceCount++;
    }

    gridInfo[x1][y1] = color;
    for (dir = 0; dir < 8; dir++) // 影响邻近8个位置
    {
        x = x1 + delta[dir][0];
        y = y1 + delta[dir][1];
        if (!inMap(x, y))
            continue;
        if (gridInfo[x][y] == -color)
        {
            effectivePoints[currCount][0] = x;
            effectivePoints[currCount][1] = y;
            currCount++;
            gridInfo[x][y] = color;
        }
    }
    if (currCount != 0)
    {
        if (color == 1)
        {
            blackPieceCount += currCount;
            whitePieceCount -= currCount;
        }
        else
        {
            whitePieceCount += currCount;
            blackPieceCount -= currCount;
        }
    }
    return true;
}

// 位操作版本的移动处理
bool procBitStep(BitGameState& state, int x0, int y0, int x1, int y1, int color) {
    int colorIdx = (color == 1) ? 1 : 0;
    
    // 检查移动是否合法
    if (!inMap(x0, y0) || !inMap(x1, y1)) 
        return false;
    if (!(state.chess[colorIdx] & funcID[x0][y0]))
        return false;
    if (state.chess[0] & funcID[x1][y1] || state.chess[1] & funcID[x1][y1])
        return false;
    
    int dx = abs(x0 - x1), dy = abs(y0 - y1);
    if ((dx == 0 && dy == 0) || dx > 2 || dy > 2)
        return false;
        
    // 如果是跳跃(5x5外围)，移除原来的棋子
    if (dx == 2 || dy == 2)
        state.chess[colorIdx] ^= funcID[x0][y0];
    
    // 添加新位置的棋子
    state.chess[colorIdx] |= funcID[x1][y1];
    
    // 同化周围的对方棋子
    long long surroundOpponents = state.chess[1-colorIdx] & around1[x1][y1];
    state.chess[colorIdx] |= surroundOpponents;
    state.chess[1-colorIdx] &= ~surroundOpponents;
    
    return true;
}

// 模拟移动(不修改实际棋盘)
bool simulateMove(GameState& state, int x0, int y0, int x1, int y1, int color) {
    if (color == 0 || !inMap(x0, y0) || !inMap(x1, y1))
        return false;
    if (state.grid[x0][y0] != color || state.grid[x1][y1] != 0)
        return false;

    int dx = abs(x0 - x1), dy = abs(y0 - y1);
    if (dx > 2 || dy > 2 || (dx == 0 && dy == 0))
        return false;

    // 复制或移动
    if (dx == 2 || dy == 2)
        state.grid[x0][y0] = 0;
    else {
        if (color == 1)
            state.blackCount++;
        else
            state.whiteCount++;
    }

    state.grid[x1][y1] = color;

    // 同化周围棋子
    int converted = 0;
    for (int dir = 0; dir < 8; dir++) {
        int nx = x1 + delta[dir][0];
        int ny = y1 + delta[dir][1];
        if (inMap(nx, ny) && state.grid[nx][ny] == -color) {
            state.grid[nx][ny] = color;
            converted++;
        }
    }

    // 更新计数
    if (color == 1) {
        state.blackCount += converted;
        state.whiteCount -= converted;
    } else {
        state.whiteCount += converted;
        state.blackCount -= converted;
    }

    return true;
}

// 使用位运算的评估函数
int bitEvaluate(const BitGameState& state, int color) {
    int colorIdx = (color == 1) ? 1 : 0;
    int oppColorIdx = 1 - colorIdx;
    
    // 计算棋子数量
    int myCount = countBits(state.chess[colorIdx]);
    int oppCount = countBits(state.chess[oppColorIdx]);
    
    // 终局判断
    if (oppCount == 0) return INT_MAX;
    if (myCount == 0) return -INT_MAX;
    
    // 棋子数量差值为主要评价标准
    int score = (myCount - oppCount) * 1000;
    
    // 计算角落和边缘位置的价值
    long long corners = (funcID[0][0] | funcID[0][6] | funcID[6][0] | funcID[6][6]);
    long long edges = 0;
    for (int i = 1; i < 6; i++) {
        edges |= funcID[0][i] | funcID[6][i] | funcID[i][0] | funcID[i][6];
    }
    
    // 角落和边缘位置的加权
    score += countBits(state.chess[colorIdx] & corners) * 500;
    score -= countBits(state.chess[oppColorIdx] & corners) * 500;
    score += countBits(state.chess[colorIdx] & edges) * 100;
    score -= countBits(state.chess[oppColorIdx] & edges) * 100;
    
    return score;
}

// 优化评估函数，减少计算量
int evaluate(const GameState& state, int color) {
    if (color == 1 && state.whiteCount == 0) return INT_MAX;
    if (color == -1 && state.blackCount == 0) return INT_MAX;
    if (color == 1 && state.blackCount == 0) return -INT_MAX;
    if (color == -1 && state.whiteCount == 0) return -INT_MAX;

    // 简化评估，主要关注棋子数量
    int score = color == 1 ? 
        (state.blackCount - state.whiteCount) * 1000 : 
        (state.whiteCount - state.blackCount) * 1000;

    // 只在角落和边缘位置计算位置权重
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            if ((i == 0 || i == 6) && (j == 0 || j == 6)) {
                // 角落位置
                if (state.grid[i][j] == color) score += 500;
                else if (state.grid[i][j] == -color) score -= 500;
            }
            else if (i == 0 || i == 6 || j == 0 || j == 6) {
                // 边缘位置
                if (state.grid[i][j] == color) score += 100;
                else if (state.grid[i][j] == -color) score -= 100;
            }
        }
    }
    
    return score;
}

// 使用位操作生成移动
vector<Move> bitGenerateMoves(const BitGameState& state, int color) {
    vector<Move> moves;
    moves.reserve(100);
    
    int colorIdx = (color == 1) ? 1 : 0;
    long long myPieces = state.chess[colorIdx];
    long long emptySpaces = ~(state.chess[0] | state.chess[1]);
    
    // 遍历自己的所有棋子
    for (int x = 0; x < 7; x++) {
        for (int y = 0; y < 7; y++) {
            if (!(myPieces & funcID[x][y]))
                continue;
                
            bool isCornerOrEdge = (x == 0 || x == 6 || y == 0 || y == 6);
            
            // 检查周围8个位置（复制）
            long long nearMoves = around1[x][y] & emptySpaces;
            while (nearMoves) {
                long long pos = nearMoves & (-nearMoves); // 获取最低位1
                nearMoves &= (nearMoves - 1); // 清除最低位1
                
                // 找到对应的x1,y1坐标
                int x1 = -1, y1 = -1;
                for (int i = 0; i < 7; i++) {
                    for (int j = 0; j < 7; j++) {
                        if (funcID[i][j] == pos) {
                            x1 = i;
                            y1 = j;
                            break;
                        }
                    }
                    if (x1 >= 0) break;
                }
                
                Move move = {x, y, x1, y1, 0};
                if (isCornerOrEdge)
                    moves.insert(moves.begin(), move);
                else
                    moves.push_back(move);
            }
            
            // 检查周围16个位置（跳跃）
            if (moves.size() < 20) {
                long long farMoves = around2[x][y] & emptySpaces;
                while (farMoves) {
                    long long pos = farMoves & (-farMoves);
                    farMoves &= (farMoves - 1);
                    
                    int x1 = -1, y1 = -1;
                    for (int i = 0; i < 7; i++) {
                        for (int j = 0; j < 7; j++) {
                            if (funcID[i][j] == pos) {
                                x1 = i;
                                y1 = j;
                                break;
                            }
                        }
                        if (x1 >= 0) break;
                    }
                    
                    moves.push_back({x, y, x1, y1, 0});
                }
            }
        }
    }
    
    return moves;
}

// 优化移动生成，优先考虑有价值的移动
vector<Move> generateMoves(const GameState& state, int color) {
    vector<Move> moves;
    moves.reserve(100); // 预分配空间

    // 优先检查角落和边缘位置的棋子
    for (int x0 = 0; x0 < 7; x0++) {
        for (int y0 = 0; y0 < 7; y0++) {
            if (state.grid[x0][y0] != color)
                continue;
                
            bool isCornerOrEdge = (x0 == 0 || x0 == 6 || y0 == 0 || y0 == 6);
            
            // 优先考虑3x3范围内的移动（复制）
            for (int dir = 0; dir < 8; dir++) {
                int x1 = x0 + delta[dir][0];
                int y1 = y0 + delta[dir][1];
                if (!inMap(x1, y1) || state.grid[x1][y1] != 0)
                    continue;
                    
                Move move = {x0, y0, x1, y1, 0};
                if (isCornerOrEdge) 
                    moves.insert(moves.begin(), move);
                else
                    moves.push_back(move);
            }
            
            // 再考虑5x5范围的移动
            if (moves.size() < 20) { // 限制移动数量
                for (int dir = 8; dir < 24; dir++) {
                    int x1 = x0 + delta[dir][0];
                    int y1 = y0 + delta[dir][1];
                    if (!inMap(x1, y1) || state.grid[x1][y1] != 0)
                        continue;
                    moves.push_back({x0, y0, x1, y1, 0});
                }
            }
        }
    }
    
    return moves;
}

// 位操作版本的Alpha-Beta剪枝搜索
int bitAlphaBeta(BitGameState& state, int depth, int alpha, int beta, bool maximizing, int color) {
    if (checkTimeout() || depth == 0 || countBits(state.chess[1]) == 0 || countBits(state.chess[0]) == 0)
        return bitEvaluate(state, color);
        
    vector<Move> moves = bitGenerateMoves(state, maximizing ? color : -color);
    if (moves.empty())
        return bitEvaluate(state, color);
        
    if (maximizing) {
        int maxEval = -INT_MAX;
        for (const Move& move : moves) {
            BitGameState newState = state;
            if (procBitStep(newState, move.startX, move.startY, move.endX, move.endY, color)) {
                int eval = bitAlphaBeta(newState, depth - 1, alpha, beta, false, color);
                maxEval = max(maxEval, eval);
                alpha = max(alpha, eval);
                if (beta <= alpha)
                    break;
            }
        }
        return maxEval;
    } else {
        int minEval = INT_MAX;
        for (const Move& move : moves) {
            BitGameState newState = state;
            if (procBitStep(newState, move.startX, move.startY, move.endX, move.endY, -color)) {
                int eval = bitAlphaBeta(newState, depth - 1, alpha, beta, true, color);
                minEval = min(minEval, eval);
                beta = min(beta, eval);
                if (beta <= alpha)
                    break;
            }
        }
        return minEval;
    }
}

// 简化的AlphaBeta搜索
int alphaBeta(GameState& state, int depth, int alpha, int beta, bool maximizing, int color) {
    if (checkTimeout() || depth == 0 || state.blackCount == 0 || state.whiteCount == 0)
        return evaluate(state, color);

    vector<Move> moves = generateMoves(state, maximizing ? color : -color);
    if (moves.empty())
        return evaluate(state, color);

    if (maximizing) {
        int maxEval = -INT_MAX;
        for (const Move& move : moves) {
            GameState newState = state;
            if (simulateMove(newState, move.startX, move.startY, move.endX, move.endY, color)) {
                int eval = alphaBeta(newState, depth - 1, alpha, beta, false, color);
                maxEval = max(maxEval, eval);
                alpha = max(alpha, eval);
                if (beta <= alpha)
                    break;
            }
        }
        return maxEval;
    } else {
        int minEval = INT_MAX;
        for (const Move& move : moves) {
            GameState newState = state;
            if (simulateMove(newState, move.startX, move.startY, move.endX, move.endY, -color)) {
                int eval = alphaBeta(newState, depth - 1, alpha, beta, true, color);
                minEval = min(minEval, eval);
                beta = min(beta, eval);
                if (beta <= alpha)
                    break;
            }
        }
        return minEval;
    }
}

// 迭代加深搜索函数
Move iterativeDeepeningSearch(int color, double timeLimit) {
    Move bestMove;
    BitGameState state;
    
    // 同步当前棋盘状态到位棋盘
    syncGridToBitBoard();
    
    // 生成所有移动
    vector<Move> moves = bitGenerateMoves(state, color);
    if (moves.empty()) return {-1, -1, -1, -1, 0};
    
    // 初始最佳移动为第一个可能的移动
    bestMove = moves[0];
    
    // 迭代加深搜索
    for (int depth = 1; depth <= 10; depth++) {
        if (checkTimeout()) break;
        
        int bestScore = -INT_MAX;
        Move currentBestMove = bestMove;
        
        for (const Move& move : moves) {
            if (checkTimeout()) break;
            
            BitGameState newState = state;
            if (procBitStep(newState, move.startX, move.startY, move.endX, move.endY, color)) {
                int score = bitAlphaBeta(newState, depth - 1, -INT_MAX, INT_MAX, false, color);
                
                if (score > bestScore) {
                    bestScore = score;
                    currentBestMove = move;
                }
            }
        }
        
        if (!checkTimeout()) {
            bestMove = currentBestMove;
        }
    }
    
    return bestMove;
}

int main()
{
    istream::sync_with_stdio(false);

    int x0, y0, x1, y1;

    // 初始化棋盘
    gridInfo[0][0] = gridInfo[6][6] = 1;  //|黑|白|
    gridInfo[6][0] = gridInfo[0][6] = -1; //|白|黑|

    // 初始化位操作
    initBit();

    // 分析自己收到的输入和自己过往的输出，并恢复状态
    int turnID;
    currBotColor = -1; // 第一回合收到坐标是-1, -1，说明我是黑方
    cin >> turnID;
    for (int i = 0; i < turnID - 1; i++)
    {
        // 根据这些输入输出逐渐恢复状态到当前回合
        cin >> x0 >> y0 >> x1 >> y1;
        if (x1 >= 0)
            ProcStep(x0, y0, x1, y1, -currBotColor); // 模拟对方落子
        else
            currBotColor = 1;
        cin >> x0 >> y0 >> x1 >> y1;
        if (x1 >= 0)
            ProcStep(x0, y0, x1, y1, currBotColor); // 模拟己方落子
    }

    // 看看自己本回合输入
    cin >> x0 >> y0 >> x1 >> y1;
    if (x1 >= 0)
        ProcStep(x0, y0, x1, y1, -currBotColor); // 模拟对方落子
    else
        currBotColor = 1;

    // 同步到位棋盘
    syncGridToBitBoard();

    // 初始化计时器
    startTime = Clock::now();

    // 使用迭代加深搜索
    Move bestMove = iterativeDeepeningSearch(currBotColor, TIME_LIMIT);
    
    if (bestMove.startX >= 0) {
        startX = bestMove.startX;
        startY = bestMove.startY;
        resultX = bestMove.endX;
        resultY = bestMove.endY;
    } else {
        // 备用方案：找出合法落子点
        int beginPos[1000][2], possiblePos[1000][2], posCount = 0, dir;

        for (y0 = 0; y0 < 7; y0++) {
            for (x0 = 0; x0 < 7; x0++) {
                if (gridInfo[x0][y0] != currBotColor)
                    continue;
                for (dir = 0; dir < 24; dir++) {
                    x1 = x0 + delta[dir][0];
                    y1 = y0 + delta[dir][1];
                    if (!inMap(x1, y1))
                        continue;
                    if (gridInfo[x1][y1] != 0)
                        continue;
                    beginPos[posCount][0] = x0;
                    beginPos[posCount][1] = y0;
                    possiblePos[posCount][0] = x1;
                    possiblePos[posCount][1] = y1;
                    posCount++;
                }
            }
        }

        // 做出决策
        if (posCount > 0) {
            GameState currentState;
            vector<Move> moves = generateMoves(currentState, currBotColor);
            
            if (!moves.empty()) {
                int bestScore = -INT_MAX;
                Move bestMove = moves[0];
                
                for (const Move& move : moves) {
                    GameState newState = currentState;
                    if (simulateMove(newState, move.startX, move.startY, move.endX, move.endY, currBotColor)) {
                        int score = alphaBeta(newState, 3, -INT_MAX, INT_MAX, false, currBotColor);
                        if (score > bestScore) {
                            bestScore = score;
                            bestMove = move;
                        }
                    }
                }
                
                startX = bestMove.startX;
                startY = bestMove.startY;
                resultX = bestMove.endX;
                resultY = bestMove.endY;
            } else {
                // 如果没有找到合法移动，使用第一个可能的移动
                startX = beginPos[0][0];
                startY = beginPos[0][1];
                resultX = possiblePos[0][0];
                resultY = possiblePos[0][1];
            }
        } else {
            startX = startY = resultX = resultY = -1;
        }
    }

    // 决策结束，输出结果
    cout << startX << " " << startY << " " << resultX << " " << resultY;
    return 0;
}