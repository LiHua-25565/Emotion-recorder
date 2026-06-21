#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <chrono>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <vector>

static constexpr int WINDOW_WIDTH = 1280;
static constexpr int WINDOW_HEIGHT = 720;
static constexpr float CROSS_MARGIN_RATIO = 0.3f; // 十字线距边的比例
static constexpr float VERTICAL_HEIGHT_RATIO = 0.7f; // 垂直线高度占比
static constexpr float HORIZONTAL_WIDTH_RATIO = 0.6f; // 水平线宽度占比

// 小圆点数据：位置 + 象限编号 (0:左上, 1:右上, 2:左下, 3:右下)
struct Dot {
    float x, y;
    int quadrant; // 0~3
};

std::string GetCurrentTimeString() {
    auto t = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &t);
    std::ostringstream oss;
    oss << std::setfill('0')
        << (1900 + tm.tm_year) << u8"年"
        << std::setw(2) << (1 + tm.tm_mon) << u8"月"
        << std::setw(2) << tm.tm_mday << u8"日  "
        << std::setw(2) << tm.tm_hour << u8":"
        << std::setw(2) << tm.tm_min << u8":"
        << std::setw(2) << tm.tm_sec;
    return oss.str();
}

std::vector<std::string> ReadFileLines(const std::string& filePath) {
    std::vector<std::string> lines;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        lines.push_back(u8"文件不存在");
        return lines;
    }
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(line);
    }
    // 如果文件为空，补一行
    if (lines.empty()) lines.push_back(u8"空文件");
    return lines;
}

void WriteDotLog(const std::string& filePath, int counts[4]) {
    std::ofstream out(filePath);
    if (!out) return;
    out << u8"左上: " << counts[0] << "\n";
    out << u8"右上: " << counts[1] << "\n";
    out << u8"左下: " << counts[2] << "\n";
    out << u8"右下: " << counts[3] << "\n";
}

void SaveDotToFile(const std::string& filePath, float x, float y, int quadrant) {
    std::ofstream out(filePath, std::ios::app); // 追加
    if (out) {
        out << x << "," << y << "," << quadrant << "\n";
    }
}

void LogDotHistory(const std::string& filePath, const std::string& quadText, const std::string& timeStr) {
    std::ofstream out(filePath, std::ios::app); 
    if (out) {
        out << timeStr << " " << quadText << "\n";
    }
}

std::vector<Dot> LoadDotsFromFile(const std::string& filePath) {
    std::vector<Dot> dots;
    std::ifstream in(filePath);
    if (!in) return dots;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        // 格式：x,y,quadrant
        size_t p1 = line.find(',');
        size_t p2 = line.rfind(',');
        if (p1 == std::string::npos || p2 == std::string::npos || p1 == p2) continue;
        try {
            float x = std::stof(line.substr(0, p1));
            float y = std::stof(line.substr(p1 + 1, p2 - p1 - 1));
            int q = std::stoi(line.substr(p2 + 1));
            dots.push_back({ x, y, q });
        }
        catch (...) {
            continue; // 忽略格式错误的行
        }
    }
    return dots;
}

void init()
{
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO);
    TTF_Init();
}

void quit()
{
    SDL_Quit();
    TTF_Quit();
}

int main(int argc, char* argv[])
{
    using namespace std::chrono;

    init();

    SDL_Window* window = SDL_CreateWindow(u8"MySTR",
        WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    TTF_Font* font = TTF_OpenFont("font/SourceHanSansSC-Bold.otf", 48);
    if (!font) {
        SDL_Log("FATAL: TTF_OpenFont failed: %s", SDL_GetError());
    }
    TTF_Font* smallFont = TTF_OpenFont("font/SourceHanSansSC-Bold.otf", 20); // 按钮用小字体
    if (!smallFont) {
        SDL_Log("Warning: small font not loaded, button text may be missing.");
    }

    TTF_TextEngine* textEngine = TTF_CreateRendererTextEngine(renderer);
    if (!textEngine) {
        SDL_Log("Failed to create text engine: %s", SDL_GetError());
        // 可退出或降级
    }

    // 创建中心文字对象（固定不变，除非要刷新）
    // 读取所有行
    std::vector<std::string> allLines = ReadFileLines("center_text.txt");
    // 确保至少有5行
    while (allLines.size() < 5) allLines.push_back(u8"无内容");

    // 加载点存档
    std::vector<Dot> dots = LoadDotsFromFile("save/dots.txt");
    int quadCounts[4] = { 0 };
    for (const auto& dot : dots) {
        if (dot.quadrant >= 0 && dot.quadrant < 4) {
            quadCounts[dot.quadrant]++;
        }
    }
    WriteDotLog("save/dot_log.txt", quadCounts);

    // 创建中心文字对象（第1行）
    TTF_Text* centerTextObj = TTF_CreateText(textEngine, font,
        allLines[0].c_str(), allLines[0].length());

    // 创建四个象限文字对象（第2~5行）
    std::string quadrantTextsRaw[4];
    for (int i = 0; i < 4; ++i) {
        quadrantTextsRaw[i] = allLines[i + 1];
    }
    TTF_Text* quadrantTexts[4];
    for (int i = 0; i < 4; ++i) {
        quadrantTexts[i] = TTF_CreateText(textEngine, font,
            allLines[i + 1].c_str(), allLines[i + 1].length());
    }
    
    // 按钮文字对象
    TTF_Text* clearBtnTextObj = TTF_CreateText(textEngine, smallFont ? smallFont : font,
        u8"清除存档", std::string(u8"清除存档").length());

    // 按钮矩形（右下角）
    const SDL_FRect clearBtnRect = {
        WINDOW_WIDTH - 140.0f,  // x
        WINDOW_HEIGHT - 60.0f,  // y
        120.0f,                 // width
        40.0f                   // height
    };

    // 重置今日限制按钮文字
    TTF_Text* resetTodayBtnTextObj = TTF_CreateText(textEngine, smallFont ? smallFont : font,
        u8"重置今日", std::string(u8"重置今日").length());

    const SDL_FRect resetTodayBtnRect = {
        WINDOW_WIDTH - 300.0f,  
        WINDOW_HEIGHT - 60.0f,
        120.0f,
        40.0f
    };

    // 初始时间文字对象
    std::string currentTimeStr = GetCurrentTimeString();
    TTF_Text* timeTextObj = TTF_CreateText(textEngine, font, currentTimeStr.c_str(), currentTimeStr.length());

    // 用于判断是否更新时间的变量
    std::string lastTimeStr = currentTimeStr;

    // 每日限制模式相关
    bool dailyLimitEnabled = false;
    std::string lastDotDate; // 存储上次加点日期（格式 YYYY-MM-DD）
    const std::string dateFileName = "save/last_dot_date.txt";

    std::ifstream settingFile("setting.txt");
    if (settingFile.is_open()) {
        std::string line;
        while (std::getline(settingFile, line)) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                // 去掉首尾空格
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);
                if (key == "one_per_day") {
                    dailyLimitEnabled = (value == "1");
                }
            }
        }
        settingFile.close();
    }

    // 如果开启了限制，读取上次加点日期
    if (dailyLimitEnabled) {
        std::ifstream dateFile(dateFileName);
        if (dateFile.is_open()) {
            std::getline(dateFile, lastDotDate);
            dateFile.close();
        }
    }

    bool is_fullscreen = false;

    // 设置逻辑分辨率
    SDL_SetRenderLogicalPresentation(
        renderer,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_LOGICAL_PRESENTATION_LETTERBOX // 等比例留黑边
    );

    SDL_Event event;
    bool is_quit = false;
    bool showSuccessMsg = false;

    const nanoseconds frame_duration(1000000000 / 144);
    steady_clock::time_point last_tick = steady_clock::now();

    while (!is_quit) {
        // 输入
        while (SDL_PollEvent(&event))
        {
            SDL_ConvertEventToRenderCoordinates(renderer, &event);

            if (event.type == SDL_EVENT_QUIT)
                is_quit = true;

            if (event.type == SDL_EVENT_KEY_DOWN)
            {
                // 监听F11按键按下
                if (event.key.key == SDLK_F11)
                {
                    is_fullscreen = !is_fullscreen;
                    SDL_SetWindowFullscreen(window, is_fullscreen);
                }
                // ESC 键
                if (event.key.key == SDLK_ESCAPE)
                {
                    is_fullscreen = false;
                    SDL_SetWindowFullscreen(window, is_fullscreen);
                }
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                float mx = event.button.x;
                float my = event.button.y;

                // 检查是否点击了清除按钮
                if (mx >= clearBtnRect.x && mx <= clearBtnRect.x + clearBtnRect.w &&
                    my >= clearBtnRect.y && my <= clearBtnRect.y + clearBtnRect.h) {

                    // 弹出确认对话框
                    const SDL_MessageBoxButtonData buttons[] = {
                        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, u8"取消" },
                        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, u8"确定" },
                    };
                    const SDL_MessageBoxData msgbox = {
                        SDL_MESSAGEBOX_WARNING,
                        window,
                        u8"确认清除",
                        u8"真的确定要删除存档吗？",
                        (int)SDL_arraysize(buttons),
                        buttons,
                        NULL
                    };
                    int buttonId;
                    if (SDL_ShowMessageBox(&msgbox, &buttonId) && buttonId == 1) {
                        // 用户点击了“确定”
                        dots.clear();
                        std::fill(quadCounts, quadCounts + 4, 0);
                        lastDotDate.clear();
                        std::ofstream(dateFileName, std::ios::trunc).close();
                        WriteDotLog("save/dot_log.txt", quadCounts);
                        std::ofstream("save/dots.txt", std::ios::trunc).close();
                        std::ofstream("save/dot_history.txt", std::ios::trunc).close();
                    }
                }
                else if (mx >= resetTodayBtnRect.x && mx <= resetTodayBtnRect.x + resetTodayBtnRect.w &&
                    my >= resetTodayBtnRect.y && my <= resetTodayBtnRect.y + resetTodayBtnRect.h) {

                    if (dailyLimitEnabled) {
                        // 清除今日限制
                        lastDotDate.clear();
                        std::ofstream(dateFileName, std::ios::trunc).close();
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                            u8"提示", u8"今日限制已重置，您可以再次添加一个点。", window);
                    }
                    else {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                            u8"提示", u8"当前未开启每日限点模式。", window);
                    }
                }
                else {
                    // 获取当前日期（仅日期部分，用于限制判断）
                    auto t = std::time(nullptr);
                    std::tm tm;
                    localtime_s(&tm, &t);
                    char todayStr[11];
                    std::strftime(todayStr, sizeof(todayStr), "%Y-%m-%d", &tm);
                    std::string today(todayStr);

                    if (dailyLimitEnabled && today == lastDotDate) {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                            u8"提示", u8"今天已经添加过点了，明日再来吧～", window);
                        continue; // 跳出当前事件处理，不执行后续加点代码
                    }

                    // 原有加点逻辑
                    int quad = -1;
                    float cx = WINDOW_WIDTH / 2.0f;
                    float cy = WINDOW_HEIGHT / 2.0f;
                    if (mx < cx && my < cy)      quad = 0;
                    else if (mx >= cx && my < cy) quad = 1;
                    else if (mx < cx && my >= cy) quad = 2;
                    else                         quad = 3;

                    dots.push_back({ mx, my, quad });
                    quadCounts[quad]++;

                    SaveDotToFile("save/dots.txt", mx, my, quad);
                    WriteDotLog("save/dot_log.txt", quadCounts);

                    // 记录点历史日志（象限文字+时间）
                    std::string nowTime = GetCurrentTimeString();
                    LogDotHistory("save/dot_history.txt", quadrantTextsRaw[quad], nowTime);

                    // 更新上次加点日期
                    if (dailyLimitEnabled) {
                        lastDotDate = today;
                        std::ofstream dateFile(dateFileName);
                        if (dateFile.is_open()) {
                            dateFile << lastDotDate;
                            dateFile.close();
                        }
                    }

                    showSuccessMsg = true;
                }
            }

        }

        // 逻辑

        std::string nowTime = GetCurrentTimeString();
        if (nowTime != lastTimeStr) {
            if (timeTextObj) TTF_DestroyText(timeTextObj);
            timeTextObj = TTF_CreateText(textEngine, font, nowTime.c_str(), nowTime.length());
            lastTimeStr = nowTime;
        }

        // 渲染

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        float cx = WINDOW_WIDTH / 2.0f;
        float cy = WINDOW_HEIGHT / 2.0f;

        if (timeTextObj) {
            int w = 0, h = 0;
            TTF_GetTextSize(timeTextObj, &w, &h);
            float x = (WINDOW_WIDTH - w) / 2.0f;
            float y = 20.0f;
            TTF_SetTextColor(timeTextObj, 0, 0, 0, 255);  // 黑色
            TTF_DrawRendererText(timeTextObj, x, y);
        }

        // 画四象限
        const float line_width = 4.0f;
        const float half_width = line_width * 0.5f;
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

        SDL_FRect vertical_line = {
            cx - half_width,
            cy - 0.3f * WINDOW_HEIGHT,
            line_width,
            0.7f * WINDOW_HEIGHT,
        };
        SDL_RenderFillRect(renderer, &vertical_line);
        SDL_FRect horizontal_line = {
            cx - 0.3f * WINDOW_WIDTH,
            cy - half_width,
            0.6f * WINDOW_WIDTH,
            line_width,
        };
        SDL_RenderFillRect(renderer, &horizontal_line);

        // 定义四个象限的中心坐标（左上、右上、左下、右下）
        float cross_left = cx - CROSS_MARGIN_RATIO * WINDOW_WIDTH;             
        float cross_right = cross_left + HORIZONTAL_WIDTH_RATIO * WINDOW_WIDTH;
        float cross_top = cy - CROSS_MARGIN_RATIO * WINDOW_HEIGHT;             
        float cross_bottom = cross_top + VERTICAL_HEIGHT_RATIO * WINDOW_HEIGHT;

        // 四个象限的中心坐标（以短十字为边界）
        float quadX[4] = {
            (cross_left + cx) / 2.0f,   // 左上
            (cx + cross_right) / 2.0f,  // 右上
            (cross_left + cx) / 2.0f,   // 左下
            (cx + cross_right) / 2.0f   // 右下
        };
        float quadY[4] = {
            (cross_top + cy) / 2.0f,    // 左上、右上
            (cross_top + cy) / 2.0f,
            (cy + cross_bottom) / 2.0f, // 左下、右下
            (cy + cross_bottom) / 2.0f
        };

        for (int i = 0; i < 4; ++i) {
            if (quadrantTexts[i]) {
                int w, h;
                TTF_GetTextSize(quadrantTexts[i], &w, &h);
                float x = quadX[i] - w / 2.0f;
                float y = quadY[i] - h / 2.0f;

                // 可选：画一个半透明或白色背景块，防止十字干扰
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_FRect bg = { x - 4, y - 2, w + 8.0f, h + 4.0f };
                SDL_RenderFillRect(renderer, &bg);

                static const SDL_Color gray = { 128, 128, 128, 255 };
                TTF_SetTextColor(quadrantTexts[i], gray.r, gray.b, gray.g, gray.a); 
                TTF_DrawRendererText(quadrantTexts[i], x, y);
            }
        }

        // 画点
        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
        for (const auto& dot : dots) {
            SDL_FRect rect = { dot.x - 5, dot.y - 5, 10, 10 };
            SDL_RenderFillRect(renderer, &rect);
        }

        // 绘制清除按钮（红色矩形 + 白色文字）
        SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
        SDL_RenderFillRect(renderer, &clearBtnRect);

        // 可选白色边框
        SDL_SetRenderDrawColor(renderer, 150, 100, 100, 255);
        SDL_RenderRect(renderer, &clearBtnRect);

        // 按钮文字
        if (clearBtnTextObj) {
            int w, h;
            TTF_GetTextSize(clearBtnTextObj, &w, &h);
            float textX = clearBtnRect.x + (clearBtnRect.w - w) / 2.0f;
            float textY = clearBtnRect.y + (clearBtnRect.h - h) / 2.0f;
            TTF_SetTextColor(clearBtnTextObj, 255, 255, 255, 255);
            TTF_DrawRendererText(clearBtnTextObj, textX, textY);
        }

        // 绘制“重置今日”按钮（蓝色矩形 + 白色文字）
        SDL_SetRenderDrawColor(renderer, 0, 120, 215, 255);  // 蓝色
        SDL_RenderFillRect(renderer, &resetTodayBtnRect);

        SDL_SetRenderDrawColor(renderer, 100, 150, 200, 255);
        SDL_RenderRect(renderer, &resetTodayBtnRect);

        if (resetTodayBtnTextObj) {
            int w, h;
            TTF_GetTextSize(resetTodayBtnTextObj, &w, &h);
            float textX = resetTodayBtnRect.x + (resetTodayBtnRect.w - w) / 2.0f;
            float textY = resetTodayBtnRect.y + (resetTodayBtnRect.h - h) / 2.0f;
            TTF_SetTextColor(resetTodayBtnTextObj, 255, 255, 255, 255);
            TTF_DrawRendererText(resetTodayBtnTextObj, textX, textY);
        }

        SDL_RenderPresent(renderer);

        if (showSuccessMsg) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,
                u8"提示", u8"添加成功！", window);
            showSuccessMsg = false;
        }
    }

    if (timeTextObj) TTF_DestroyText(timeTextObj);
    for (int i = 0; i < 4; ++i) {
        if (quadrantTexts[i]) TTF_DestroyText(quadrantTexts[i]);
    }
    if (centerTextObj) TTF_DestroyText(centerTextObj);
    if (clearBtnTextObj) TTF_DestroyText(clearBtnTextObj);
    if (resetTodayBtnTextObj) TTF_DestroyText(resetTodayBtnTextObj);
    TTF_DestroyRendererTextEngine(textEngine);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    quit();

    return 0;
}