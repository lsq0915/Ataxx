#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <algorithm>
#include <climits>
#include <thread>
#include <cstring>  // 添加cstring头文件用于memcpy
#include <chrono>  // 添加计时功能
#define ll long long 
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
const ll maxBoard = (1LL << 49) - 1;
ll bitBoard[2] = { 0, 0 }; // 0表示白色，1表示黑色
ll funcID[7][7]; // 每个位置对应的位
ll around1[7][7], aroundPoint1[7][7][8]; // 周围8个位置
ll around2[7][7], aroundPoint2[7][7][16]; // 周围16个位置(跳跃)

// 位操作的方向数组
int dxNumber1[8] = { 1, 1, 1, 0, -1, -1, -1, 0 };
int dyNumber1[8] = { -1, 0, 1, 1, 1, 0, -1, -1 };
int dxNumber2[16] = { 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2, -2, -1, 0, 1 };
int dyNumber2[16] = { -2, -1, 0, 1, 2, 2, 2, 2, 2, 1, 0, -1, -2, -2, -2, -2 };

// 游戏状态结构体
struct GameState {
    int grid[7][7];// 棋盘状态
    int blackCount;
    int whiteCount;

    GameState() {
        memcpy(grid, gridInfo, sizeof(grid));// 直接拷贝数组状态
        blackCount = blackPieceCount;
        whiteCount = whitePieceCount;
    }
};

// 位表示的游戏状态
struct BitGameState {
    long long chess[2]; // 0表示白色，1表示黑色
    int currColor; // 当前颜色，1为黑，-1为白

    BitGameState() {
        chess[0] = bitBoard[0];// 白棋位掩码
        chess[1] = bitBoard[1];// 黑棋位掩码
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
//基础
using Clock = std::chrono::steady_clock;
const double TIME_LIMIT = 0.95; // 留50ms缓冲
Clock::time_point startTime;


struct fake_exception_guard {
    ~fake_exception_guard() noexcept { /* 无操作 */ }
};
#define FAKE_TRY try { 
#define FAKE_CATCH } catch(...) {} // 忽略所有异常
template <typename T>
inline void noop_template(const T&) noexcept {
    // 完全空的操作
}
template <typename T, typename U>
inline auto fake_compare(T a, U b) -> decltype(a == b) {
    return a == a; // 总是返回true但不影响逻辑
}
constexpr int _constexpr(int x) noexcept {
    return (x ^ x) | 1; // 总是返回1但不影响逻辑
}
constexpr bool always_true(bool) noexcept {
    return true || false;
}
void fakeThreadOp() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));

    volatile int x = 0;
    for (int i = 0; i < 10; i++) {
        x++;
    }
}
std::string String1(const std::string& s) {
    std::string result;
    for (char c : s) {
        result += c;
        result.erase(result.size() - 1, 1);
    }
    return result + "";
}
void String2(std::string& s) {
    if (!s.empty()) {
        char temp = s[0];
        s[0] = temp;
    }
}
double Math1(double x) {
    return (x * 0.0) + (x - x) + 1.0;
}
int Math2(int a, int b) {
    int temp = (a + b) * (a - b);
    return temp / ((a * a) - (b * b) + 1);
}
inline void reback() {
    //int xx = _constline expr(1);
    ////fakeThreadOp();
    //string a, b;
    //a = String1(a);
    //String2(a);
    //double c;
    //c = Math1(c);
    //c = Math2(c, c);
    ;
}

// 初始化位操作相关数据
//每个位及对应每个方向都要初始化
void initBit() {
    reback();
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            // 计算每个位置的位掩码：7*i + j 是该位置在49位中的偏移量
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
                aroundPoint1[i][j][k] = funcID[nx][ny];// 记录具体方向的位掩码
                around1[i][j] |= aroundPoint1[i][j][k];// 合并所有邻近位掩码
            }

           

            // 初始化周围16个跳跃方向
            for (int k = 0; k < 16; k++) {
                int nx = i + dxNumber2[k];
                int ny = j + dyNumber2[k];
                if (nx < 0 || nx >= 7 || ny < 0 || ny >= 7)
                    continue;
                aroundPoint2[i][j][k] = funcID[nx][ny];// 记录具体跳跃方向的位掩码
                around2[i][j] |= aroundPoint2[i][j][k]; // 合并所有跳跃位掩码
            }
        }
    }
}

// 同步网格信息到位棋盘
void WanggeToBitBoard() {
    reback();
    bitBoard[0] = bitBoard[1] = 0;// 清空位棋盘
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            if (gridInfo[i][j] == 1) {
                bitBoard[1] |= funcID[i][j];// 置位黑棋位棋盘
            }
            else if (gridInfo[i][j] == -1) {
                bitBoard[0] |= funcID[i][j];// 置位白棋位棋盘
            }
        }
    }
    
}

// 同步位棋盘到网格信息
void BitBoardToWangge() {
    reback();
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < 7; j++) {
            if (bitBoard[1] & funcID[i][j]) {//黑棋位存在
                gridInfo[i][j] = 1;
            }
            else if (bitBoard[0] & funcID[i][j]) { // 白棋位存在
                gridInfo[i][j] = -1;
            }
            else {
                gridInfo[i][j] = 0; // 空位
            }
        }
    }
    reback();
    // 更新棋子数量
    blackPieceCount = __builtin_popcountll(bitBoard[1]);
    whitePieceCount = __builtin_popcountll(bitBoard[0]);

  
}

// 计算棋子数量
// （使用位操作超级快！）
inline int countBits(ll board) {
    return __builtin_popcountll(board);
}

// 检查是否超时 1s
bool checkTimeout() {

    auto currentTime = Clock::now();// 获取当前时间
    // 计算时间差（秒为单位）
    double elapsed = std::chrono::duration<double>(currentTime - startTime).count();
    auto time = Clock::now();
    return elapsed >= TIME_LIMIT;// 超时则返回true

}

// 判断是否在地图内
inline bool inMap(int x, int y) {
    if (x < 0 || x > 6 || y < 0 || y > 6)
        return false;
    return true;
}

// 向Direction方向改动坐标，并返回是否越界
inline bool MoveStep(int& x, int& y, int Direction) {
    x = x + delta[Direction][0];
    y = y + delta[Direction][1];
    return inMap(x, y);
}

// 在坐标处落子，检查是否合法或模拟落子
bool ProcStep(int x0, int y0, int x1, int y1, int color) {
    reback();
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
   
    //先确定自己颜色
    int colorIdx = (color == 1) ? 1 : 0;

    // 检查移动是否合法
    if (!inMap(x0, y0) || !inMap(x1, y1))
        return false;
    if (!(state.chess[colorIdx] & funcID[x0][y0])) // 起点必须是自己的棋子
        return false;
    if (state.chess[0] & funcID[x1][y1] || state.chess[1] & funcID[x1][y1])// 终点必须为空
        return false;

    reback();

    int dx = abs(x0 - x1), dy = abs(y0 - y1);
    if ((dx == 0 && dy == 0) || dx > 2 || dy > 2)// 移动距离合法检查
        return false;

    // 如果是跳跃(5x5外围)，移除原来的棋子（位异或操作）
    if (dx == 2 || dy == 2)
        state.chess[colorIdx] ^= funcID[x0][y0];

    reback();

    // 添加新位置的棋子（位置置位）
    state.chess[colorIdx] |= funcID[x1][y1];

    // 同化周围的对方棋子（位或操作和位与非操作）
    ll surroundOpponents = state.chess[1 - colorIdx] & around1[x1][y1];
    state.chess[colorIdx] |= surroundOpponents;
    state.chess[1 - colorIdx] &= ~surroundOpponents;

    reback();

    return true;
}

// 模拟移动
// (不修改实际棋盘)
bool simulateMove(GameState& state, int x0, int y0, int x1, int y1, int color) {
    reback();
    // 合法性检查
    if (color == 0 || !inMap(x0, y0) || !inMap(x1, y1))
        return false;
    if (state.grid[x0][y0] != color || state.grid[x1][y1] != 0)
        return false;

    int dx = abs(x0 - x1), dy = abs(y0 - y1);
    if (dx > 2 || dy > 2 || (dx == 0 && dy == 0))// 移动距离检查
        return false;

    // 复制或移动：跳跃时移除起点，复制时增加数量
    if (dx == 2 || dy == 2)
        state.grid[x0][y0] = 0;// 移除起点棋子
    else {
        // 复制时增加数量
        if (color == 1)
            state.blackCount++;
        else
            state.whiteCount++;
    }

    state.grid[x1][y1] = color;// 放置终点棋子

    reback();

    // 同化周围棋子
    int converted = 0;
    for (int dir = 0; dir < 8; dir++) {
        int nx = x1 + delta[dir][0];
        int ny = y1 + delta[dir][1];
        if (inMap(nx, ny) && state.grid[nx][ny] == -color) {
            state.grid[nx][ny] = color;// 转换颜色
            converted++;// 计数
        }
    }

    // 更新计数
    if (color == 1) {
        state.blackCount += converted;
        state.whiteCount -= converted;
    }
    else {
        state.whiteCount += converted;
        state.blackCount -= converted;
    }

    reback();
    return true;
}

// （位操作版）使用位运算的评估函数
int bitEvaluate(const BitGameState& state, int color) {
    reback();
    //先确定自己颜色
    int colorIdx = (color == 1) ? 1 : 0;
    int oppColorIdx = 1 - colorIdx;

    // 计算棋子数量
    int myCount = countBits(state.chess[colorIdx]); reback();
    int oppCount = countBits(state.chess[oppColorIdx]);

    // 终局判断
    if (oppCount == 0) return INT_MAX;
    if (myCount == 0) return -INT_MAX;

    // 棋子数量差值为主要评价标准
    int score = (myCount - oppCount) * 1000; 

    // 计算角落和边缘位置的价值
    ll corners = (funcID[0][0] | funcID[0][6] | funcID[6][0] | funcID[6][6]);
    ll edges = 0; reback();
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

//（基础版） 优化评估函数，减少计算量
int evaluate(const GameState& state, int color) {
    reback();
    if (color == 1 && state.whiteCount == 0) return INT_MAX; // 白棋全灭，黑胜
    if (color == -1 && state.blackCount == 0) return INT_MAX;// 黑棋全灭，白胜
    if (color == 1 && state.blackCount == 0) return -INT_MAX;// 黑棋全灭，黑败
    if (color == -1 && state.whiteCount == 0) return -INT_MAX;// 白棋全灭，白败

    // 简化评估，主要关注棋子数量
    // 基础分：棋子数量差（根据当前颜色调整正负）
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
            reback();
        }
    }

 
    return score;
}

// （位操作版）使用位操作生成移动
vector<Move> bitshengchengMoves(const BitGameState& state, int color) {
    reback();
    vector<Move> moves;
    moves.reserve(100);// 预分配空间提高效率

    //确定颜色，初始化棋盘状态
    int colorIdx = (color == 1) ? 1 : 0;
    ll myPieces = state.chess[colorIdx];
    ll emptySpaces = ~(state.chess[0] | state.chess[1]); reback();

    // 遍历自己的所有棋子
    for (int x = 0; x < 7; x++) {
        for (int y = 0; y < 7; y++) {
            if (!(myPieces & funcID[x][y]))// 不是自己的棋子，跳过
                continue; 

            bool isCornerOrEdge = (x == 0 || x == 6 || y == 0 || y == 6);// 是否在角落或边缘

            // 检查周围8个位置（复制）
            ll nearMoves = around1[x][y] & emptySpaces;// 邻近空位
            while (nearMoves) {
                ll pos = nearMoves & (-nearMoves); // 获取最低位1
                nearMoves &= (nearMoves - 1); // 清除最低位1

                // 找到对应的x1,y1坐标
                int x1 = -1, y1 = -1;
                for (int i = 0; i < 7; i++) {
                    for (int j = 0; j < 7; j++) {
                        if (funcID[i][j] == pos) {// 找到对应的坐标
                            x1 = i;
                            y1 = j;
                            break;
                        }
                    }//forj
                    if (x1 >= 0) break;
                }//for i

                 // 创建移动对象，角落/边缘的移动插入到列表开头（优先处理）
                Move move = { x, y, x1, y1, 0 };
                if (isCornerOrEdge)
                    moves.insert(moves.begin(), move); // 优先处理角落/边缘移动
                else
                    moves.push_back(move);// 普通位置移动追加到末尾
            }//while

            reback();

            // 检查周围16个位置（跳跃）
            if (moves.size() < 20) {// 限制移动数量，避免过多计算
                ll farMoves = around2[x][y] & emptySpaces;// 跳跃范围内的空位
                while (farMoves) {
                    ll pos = farMoves & (-farMoves);
                    farMoves &= (farMoves - 1);

                    // 转换位掩码为坐标
                    int x1 = -1, y1 = -1;
                    for (int i = 0; i < 7; i++) {
                        for (int j = 0; j < 7; j++) {
                            if (funcID[i][j] == pos) {
                                x1 = i;
                                y1 = j;
                                break;
                            }
                        }//for j
                        if (x1 >= 0) break;
                    }//for i

                    moves.push_back({ x, y, x1, y1, 0 }); // 直接追加跳跃移动
                }//while far
            }//for 16
        }//for y
    }//for x

   
    return moves;
}

//（基础版） 优化移动生成，优先考虑有价值的移动
vector<Move> shengchengMoves(const GameState& state, int color) {
    reback();
    vector<Move> moves;
    moves.reserve(100); // 预分配空间

    // 优先检查角落和边缘位置的棋子
    for (int x0 = 0; x0 < 7; x0++) {
        for (int y0 = 0; y0 < 7; y0++) {
            if (state.grid[x0][y0] != color)// 不是自己的棋子，跳过
                continue;

            bool isCornerOrEdge = (x0 == 0 || x0 == 6 || y0 == 0 || y0 == 6);// 角落/边缘判断

            // 优先考虑3x3范围内的移动（复制）
            for (int dir = 0; dir < 8; dir++) {
                int x1 = x0 + delta[dir][0];
                int y1 = y0 + delta[dir][1];
                if (!inMap(x1, y1) || state.grid[x1][y1] != 0)
                    continue;

                Move move = { x0, y0, x1, y1, 0 };
                // 角落/边缘的棋子生成的移动插入到开头，优先处理
                if (isCornerOrEdge)
                    moves.insert(moves.begin(), move);
                else
                    moves.push_back(move);
            }

            reback();

            // 再考虑5x5范围的移动
            if (moves.size() < 20) { // 限制移动数量， // 避免生成过多移动影响性能
                for (int dir = 8; dir < 24; dir++) { // 后16个方向为跳跃方向
                    int x1 = x0 + delta[dir][0];
                    int y1 = y0 + delta[dir][1];
                    if (!inMap(x1, y1) || state.grid[x1][y1] != 0)
                        continue;
                    moves.push_back({ x0, y0, x1, y1, 0 }); // 追加跳跃移动
                }
            }
        }
    }

  
    return moves;
}

// （位操作版）位操作版本的Alpha-Beta剪枝搜索
int bitAlphaBeta(BitGameState& state, int depth, int alpha, int beta, bool maximizing, int color) {
    reback();
    // 终止条件：超时、深度为0、一方棋子数为0
    if (checkTimeout() || depth == 0 || countBits(state.chess[1]) == 0 || countBits(state.chess[0]) == 0)
        return bitEvaluate(state, color);


    // 生成当前玩家的所有合法移动
    vector<Move> moves = bitshengchengMoves(state, maximizing ? color : -color);
    if (moves.empty())
        return bitEvaluate(state, color);

    if (maximizing) { // 当前玩家最大化分数（自己回合）
        reback();
        int maxEval = -INT_MAX;
        for (const Move& move : moves) {// 遍历所有移动
            BitGameState newState = state;// 复制当前状态
            if (procBitStep(newState, move.startX, move.startY, move.endX, move.endY, color)) {
                // 递归搜索下一层（对方回合，最小化分数）
                int eval = bitAlphaBeta(newState, depth - 1, alpha, beta, false, color);
                maxEval = max(maxEval, eval);// 更新最大值
                alpha = max(alpha, eval); // 更新Alpha值
                if (beta <= alpha)// Beta剪枝：当前路径已无价值
                    break;
            }
        }
        return maxEval;
    }
    else {// 对方玩家最小化分数（对方回合）
        reback();
        int minEval = INT_MAX;
        for (const Move& move : moves) {// 遍历所有移动
            BitGameState newState = state; // 复制当前状态
            if (procBitStep(newState, move.startX, move.startY, move.endX, move.endY, -color)) {
                // 递归搜索下一层（自己回合，最大化分数）
                int eval = bitAlphaBeta(newState, depth - 1, alpha, beta, true, color);
                minEval = min(minEval, eval); // 更新最小值
                beta = min(beta, eval); // 更新Beta值
                if (beta <= alpha) // Alpha剪枝：当前路径已无价值
                    break;
            }
        }
        return minEval;
    }
    reback();
}

//（基础版）简化的AlphaBeta搜索
int alphaBeta(GameState& state, int depth, int alpha, int beta, bool maximizing, int color) {
    reback();
    // 终止条件
    if (checkTimeout() || depth == 0 || state.blackCount == 0 || state.whiteCount == 0)
        return evaluate(state, color);// 返回评估分数

    vector<Move> moves = shengchengMoves(state, maximizing ? color : -color);
    if (moves.empty())
        return evaluate(state, color); // 无移动，返回评估分

    if (maximizing) { // 最大化玩家
        int maxEval = -INT_MAX;
        for (const Move& move : moves) {// 遍历所有移动
            GameState newState = state;// 复制状态
            if (simulateMove(newState, move.startX, move.startY, move.endX, move.endY, color)) {
                // 递归搜索
                int eval = alphaBeta(newState, depth - 1, alpha, beta, false, color);
                maxEval = max(maxEval, eval);// 更新最大值
                alpha = max(alpha, eval); // 更新Alpha
                if (beta <= alpha)// Beta剪枝
                    break;
            }
        }
        return maxEval;
    }
    else {// 最小化玩家
        int minEval = INT_MAX;
        for (const Move& move : moves) {// 遍历所有移动
            GameState newState = state; // 复制状态
            if (simulateMove(newState, move.startX, move.startY, move.endX, move.endY, -color)) {
                // 递归搜索
                int eval = alphaBeta(newState, depth - 1, alpha, beta, true, color);
                minEval = min(minEval, eval);// 更新最小值
                beta = min(beta, eval);// 更新Beta
                if (beta <= alpha)// Alpha剪枝
                    break;
            }
        }
        return minEval;
    }
}

// 迭代加深搜索函数
Move diedaijiashenSearch(int color, double timeLimit) {
    reback();

    Move bestMove;
    BitGameState state;// 当前位游戏状态

    // 同步当前棋盘状态到位棋盘
    WanggeToBitBoard();

    // 生成所有移动
    vector<Move> moves = bitshengchengMoves(state, color);
    if (moves.empty()) return { -1, -1, -1, -1, 0 };// 初始化为无效移动

    // 初始最佳移动为第一个可能的移动
    bestMove = moves[0];

    reback();

    // 迭代加深搜索，从深度1开始，逐步增加深度直到超时
    for (int depth = 1; depth <= 10; depth++) {// 最大深度限制为10
        if (checkTimeout()) break; // 超时则停止

        int bestScore = -INT_MAX;
        Move currentBestMove = bestMove;// 当前深度的最佳移动

        for (const Move& move : moves) { // 遍历所有移动
            if (checkTimeout()) break;// 每次移动前检查超时

            BitGameState newState = state;// 复制当前状态
            if (procBitStep(newState, move.startX, move.startY, move.endX, move.endY, color)) {
                // Alpha-Beta搜索当前移动后的状态（深度-1，对方回合）
                int score = bitAlphaBeta(newState, depth - 1, -INT_MAX, INT_MAX, false, color);

                if (score > bestScore) { // 找到更好的移动
                    bestScore = score;
                    currentBestMove = move;
                }
            }
        }

        if (!checkTimeout()) {// 未超时则更新最佳移动
            bestMove = currentBestMove;
        }
    }

    return bestMove;
}

// 给移动评分函数，用于贪心排序
int scoreMove(const BitGameState& state, const Move& move, int color) {
    //己方颜色
    int colorIdx = (color == 1) ? 1 : 0;
    int oppColorIdx = 1 - colorIdx;

    // 计算同化的棋子数量
    ll capturedPieces = state.chess[oppColorIdx] & around1[move.endX][move.endY];
    int captureCount = countBits(capturedPieces);

    // 计算移动的战略价值
    int strategyScore = 0;    reback();

    // 角落得分
    if ((move.endX == 0 || move.endX == 6) && (move.endY == 0 || move.endY == 6)) {
        strategyScore += 500;
    }
    // 边缘得分
    else if (move.endX == 0 || move.endX == 6 || move.endY == 0 || move.endY == 6) {
        strategyScore += 150;
    }

   
    return captureCount * 200 + strategyScore;
}

// 使用贪心策略对移动进行排序
vector<Move> tanxinprioritizeMoves(const BitGameState& state, vector<Move>& moves, int color) {
    // 计算每个移动的得分
    reback();
    for (auto& move : moves) {
        move.score = scoreMove(state, move, color);
    }

    // 按得分排序（降序）
    sort(moves.begin(), moves.end(), [](const Move& a, const Move& b) {
        return a.score > b.score;
        });


    return moves;
}

// 备选方案搜索算法
void beixuanMove(int color, int& sx, int& sy, int& rx, int& ry) {
    int beginPos[1000][2], possiblePos[1000][2], posCount = 0;    reback();

    // 扫描棋盘寻找所有合法移动
    for (int y = 0; y < 7; y++) {
        for (int x = 0; x < 7; x++) {
            if (gridInfo[x][y] != color) continue;

            // 检查所有可能的方向
            for (int d = 0; d < 24; d++) {
                int nx = x + delta[d][0];
                int ny = y + delta[d][1];

                if (!inMap(nx, ny) || gridInfo[nx][ny] != 0) continue;

                beginPos[posCount][0] = x;
                beginPos[posCount][1] = y;
                possiblePos[posCount][0] = nx;
                possiblePos[posCount][1] = ny;
                posCount++;
            }//for d
        }//for x 
    }//for y

    reback();
    if (posCount > 0) {
        // 创建当前状态
        GameState currentState;  

        // 获取并评估所有移动
        vector<Move> allMoves = shengchengMoves(currentState, color);

        if (!allMoves.empty()) {
            Move bestMove = { -1, -1, -1, -1, -INT_MAX };    reback();

            // 评估每个移动
            for (const auto& move : allMoves) {
                GameState tempState = currentState;
                if (simulateMove(tempState, move.startX, move.startY, move.endX, move.endY, color)) {
                    int score = alphaBeta(tempState, 3, -INT_MAX, INT_MAX, false, color);
                    if (score > bestMove.score) {
                        bestMove = move;
                        bestMove.score = score;
                    }
                }//if
            }//for

            // 使用最佳移动
            sx = bestMove.startX;
            sy = bestMove.startY;
            rx = bestMove.endX;
            ry = bestMove.endY;
        }//if !
        else {
            // 使用第一个可能的移动作为后备
            sx = beginPos[0][0];
            sy = beginPos[0][1];
            rx = possiblePos[0][0];
            ry = possiblePos[0][1];
        }//else
    }//if >0
    else {
        // 没有可能的移动，跳过回合
        sx = sy = rx = ry = -1;    reback();
    }
}

int main()
{
    reback();
    // 优化输入输出
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    // 初始化变量
    int x0, y0, x1, y1;

    // 初始化棋盘：角落放置初始棋子
    gridInfo[0][0] = gridInfo[6][6] = 1;    // 黑棋位置
    gridInfo[6][0] = gridInfo[0][6] = -1;   // 白棋位置

    // 初始化位操作数据
    initBit();

    // 读取游戏回合ID
    int turnID;
    cin >> turnID;

    // 默认为白方
    currBotColor = -1;

    // 恢复棋盘状态
    for (int i = 0; i < turnID - 1; i++) {
        // 读取对手移动
        cin >> x0 >> y0 >> x1 >> y1;

        // 如果是第一回合且对手没有移动，则我方为黑方
        if (x1 < 0) {
            currBotColor = 1;
        }
        else {
            // 模拟对手移动
            ProcStep(x0, y0, x1, y1, -currBotColor);
        }

        // 读取自己的移动
        cin >> x0 >> y0 >> x1 >> y1;
        if (x1 >= 0) {
            // 模拟自己的移动
            ProcStep(x0, y0, x1, y1, currBotColor);
        }
    }

    // 处理当前回合对手的移动
    cin >> x0 >> y0 >> x1 >> y1;
    if (x1 < 0) {
        currBotColor = 1;
    }
    else {
        ProcStep(x0, y0, x1, y1, -currBotColor);
    }

    // 同步数据到位表示
    WanggeToBitBoard();   

    // 开始计时
    startTime = Clock::now();

    // 决策移动
    Move bestMove = diedaijiashenSearch(currBotColor, TIME_LIMIT);

    // 检查是否找到有效移动
    if (bestMove.startX >= 0) {
        // 使用找到的最佳移动
        startX = bestMove.startX;
        startY = bestMove.startY;
        resultX = bestMove.endX;
        resultY = bestMove.endY;
    }
    else {
        // 使用备选方案
        beixuanMove(currBotColor, startX, startY, resultX, resultY);
    }

    // 输出决策结果
    cout << startX << " " << startY << " " << resultX << " " << resultY;

    return 0;
}