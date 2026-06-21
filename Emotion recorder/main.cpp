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
static constexpr float CROSS_MARGIN_RATIO = 0.3f;
static constexpr float VERTICAL_HEIGHT_RATIO = 0.7f;
static constexpr float HORIZONTAL_WIDTH_RATIO = 0.6f;

struct Dot {
    float x, y;
    int quadrant; // 0:左上, 1:右上, 2:左下, 3:右下
};

// ── 工具函数 ──────────────────────────────
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
    while (std::getline(file, line)) lines.push_back(line);
    if (lines.empty()) lines.push_back(u8"空文件");
    return lines;
}

void WriteDotLog(const std::string& filePath, int counts[4]) {
    std::ofstream out(filePath);
    if (!out) return;
    out << u8"左上: " << counts[0] << "\n"
        << u8"右上: " << counts[1] << "\n"
        << u8"左下: " << counts[2] << "\n"
        << u8"右下: " << counts[3] << "\n";
}

void LogDotHistory(const std::string& filePath, const std::string& quadText, const std::string& timeStr) {
    std::ofstream out(filePath, std::ios::app);
    if (out) out << timeStr << " " << quadText << "\n";
}

void RemoveLastHistoryLine(const std::string& filePath) {
    std::ifstream in(filePath);
    if (!in) return;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    in.close();
    if (!lines.empty()) lines.pop_back();
    std::ofstream out(filePath);
    if (out) for (const auto& l : lines) out << l << "\n";
}

// ── 统一数据文件（仅保留 DOTS 和 LAST_DOT_DATE）──
static const std::string DATA_FILE = "save/data.txt";

void SaveAllDataToFile(const std::vector<Dot>& dots, const std::string& lastDotDate) {
    std::ofstream out(DATA_FILE);
    if (!out) return;
    out << "[DOTS]\n";
    for (const auto& d : dots)
        out << d.x << "," << d.y << "," << d.quadrant << "\n";
    out << "[LAST_DOT_DATE]\n" << lastDotDate << "\n";
}

void LoadAllDataFromFile(std::vector<Dot>& dots, std::string& lastDotDate) {
    dots.clear();
    lastDotDate.clear();
    std::ifstream in(DATA_FILE);
    if (!in) return;

    std::string line;
    enum Section { NONE, DOTS, LAST_DATE };
    Section section = NONE;
    while (std::getline(in, line)) {
        if (line == "[DOTS]") { section = DOTS; continue; }
        if (line == "[LAST_DOT_DATE]") { section = LAST_DATE; continue; }
        if (line.empty()) continue;

        switch (section) {
        case DOTS: {
            size_t p1 = line.find(',');
            size_t p2 = line.rfind(',');
            if (p1 != std::string::npos && p2 != std::string::npos && p1 != p2) {
                try {
                    float x = std::stof(line.substr(0, p1));
                    float y = std::stof(line.substr(p1 + 1, p2 - p1 - 1));
                    int q = std::stoi(line.substr(p2 + 1));
                    dots.push_back({ x, y, q });
                }
                catch (...) {}
            }
            break;
        }
        case LAST_DATE:
            lastDotDate = line;
            section = NONE;
            break;
        default: break;
        }
    }
}

void init() {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO);
    TTF_Init();
}

void quit() {
    SDL_Quit();
    TTF_Quit();
}

int main(int argc, char* argv[]) {
    using namespace std::chrono;
    init();
    SDL_CreateDirectory("save");

    SDL_Window* window = SDL_CreateWindow(u8"MySTR", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    TTF_Font* font = TTF_OpenFont("font/SourceHanSansSC-Bold.otf", 48);
    if (!font) SDL_Log("FATAL: TTF_OpenFont failed: %s", SDL_GetError());
    TTF_Font* smallFont = TTF_OpenFont("font/SourceHanSansSC-Bold.otf", 20);
    if (!smallFont) SDL_Log("Warning: small font not loaded");

    TTF_TextEngine* textEngine = TTF_CreateRendererTextEngine(renderer);
    if (!textEngine) SDL_Log("Failed to create text engine: %s", SDL_GetError());

    // 读取中心文字文件，如果不存在或不足五行则自动创建
    std::ifstream testFile("center_text.txt");
    bool createDefault = !testFile.is_open();
    testFile.close();
    if (!createDefault) {
        // 文件存在，检查行数
        std::ifstream in("center_text.txt");
        int lineCount = 0;
        std::string dummy;
        while (std::getline(in, dummy)) lineCount++;
        in.close();
        if (lineCount < 5) createDefault = true;
    }

    if (createDefault) {
        std::ofstream out("center_text.txt");
        if (out) {
            out << u8"核心标语\n";
            out << u8"平淡高兴\n";
            out << u8"强烈高兴\n";
            out << u8"平淡难过\n";
            out << u8"强烈难过\n";
        }
    }

    std::vector<std::string> allLines = ReadFileLines("center_text.txt");
    while (allLines.size() < 5) allLines.push_back(u8"无内容");

    // 加载统一数据文件
    std::vector<Dot> dots;
    std::string lastDotDate;
    LoadAllDataFromFile(dots, lastDotDate);

    int quadCounts[4] = { 0 };
    for (const auto& dot : dots)
        if (dot.quadrant >= 0 && dot.quadrant < 4)
            quadCounts[dot.quadrant]++;
    WriteDotLog("save/dot_log.txt", quadCounts);

    // 创建文字对象
    TTF_Text* centerTextObj = TTF_CreateText(textEngine, font, allLines[0].c_str(), allLines[0].length());
    std::string quadrantTextsRaw[4];
    TTF_Text* quadrantTexts[4];
    for (int i = 0; i < 4; ++i) {
        quadrantTextsRaw[i] = allLines[i + 1];
        quadrantTexts[i] = TTF_CreateText(textEngine, font, allLines[i + 1].c_str(), allLines[i + 1].length());
    }

    TTF_Text* clearBtnTextObj = TTF_CreateText(textEngine, smallFont ? smallFont : font,
        u8"清除存档", std::string(u8"清除存档").length());
    TTF_Text* resetTodayBtnTextObj = TTF_CreateText(textEngine, smallFont ? smallFont : font,
        u8"重置今日", std::string(u8"重置今日").length());

    const SDL_FRect clearBtnRect = { WINDOW_WIDTH - 140.0f, WINDOW_HEIGHT - 60.0f, 120.0f, 40.0f };
    const SDL_FRect resetTodayBtnRect = { WINDOW_WIDTH - 300.0f, WINDOW_HEIGHT - 60.0f, 120.0f, 40.0f };

    std::string currentTimeStr = GetCurrentTimeString();
    TTF_Text* timeTextObj = TTF_CreateText(textEngine, font, currentTimeStr.c_str(), currentTimeStr.length());
    std::string lastTimeStr = currentTimeStr;

    // 每日限制设置
    bool dailyLimitEnabled = false;
    std::ifstream settingFile("setting.txt");
    if (settingFile.is_open()) {
        std::string line;
        while (std::getline(settingFile, line)) {
            size_t eqPos = line.find('=');
            if (eqPos != std::string::npos) {
                std::string key = line.substr(0, eqPos);
                std::string value = line.substr(eqPos + 1);
                // 修剪首尾空白
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
    else {
        std::ofstream out("setting.txt");
        if (out) {
            out << "one_per_day=1\n";
            dailyLimitEnabled = true;
        }
    }

    bool is_fullscreen = false;
    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_Event event;
    bool is_quit = false;
    bool showSuccessMsg = false;

    while (!is_quit) {
        while (SDL_PollEvent(&event)) {
            SDL_ConvertEventToRenderCoordinates(renderer, &event);
            if (event.type == SDL_EVENT_QUIT) is_quit = true;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_F11) {
                    is_fullscreen = !is_fullscreen;
                    SDL_SetWindowFullscreen(window, is_fullscreen);
                }
                if (event.key.key == SDLK_ESCAPE) {
                    is_fullscreen = false;
                    SDL_SetWindowFullscreen(window, is_fullscreen);
                }
            }

            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                float mx = event.button.x, my = event.button.y;

                // ── 清除存档 ─────────────────
                if (mx >= clearBtnRect.x && mx <= clearBtnRect.x + clearBtnRect.w &&
                    my >= clearBtnRect.y && my <= clearBtnRect.y + clearBtnRect.h) {
                    const SDL_MessageBoxButtonData buttons[] = {
                        { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, u8"取消" },
                        { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, u8"确定" },
                    };
                    const SDL_MessageBoxData msgbox = {
                        SDL_MESSAGEBOX_WARNING, window, u8"确认清除", u8"真的确定要删除存档吗？",
                        SDL_arraysize(buttons), buttons, NULL
                    };
                    int buttonId;
                    if (SDL_ShowMessageBox(&msgbox, &buttonId) && buttonId == 1) {
                        dots.clear();
                        std::fill(quadCounts, quadCounts + 4, 0);
                        lastDotDate.clear();
                        SaveAllDataToFile(dots, lastDotDate);
                        WriteDotLog("save/dot_log.txt", quadCounts);
                        std::ofstream("save/dot_history.txt", std::ios::trunc).close();
                    }
                }
                // ── 重置今日 ─────────────────
                else if (mx >= resetTodayBtnRect.x && mx <= resetTodayBtnRect.x + resetTodayBtnRect.w &&
                    my >= resetTodayBtnRect.y && my <= resetTodayBtnRect.y + resetTodayBtnRect.h) {
                    if (!dailyLimitEnabled) {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, u8"提示", u8"当前未开启每日限点模式。", window);
                    }
                    else {
                        // 检查今天是否已添加了点
                        auto t = std::time(nullptr);
                        std::tm tm;
                        localtime_s(&tm, &t);
                        char todayStr[11];
                        std::strftime(todayStr, sizeof(todayStr), "%Y-%m-%d", &tm);
                        std::string today(todayStr);

                        if (lastDotDate == today && !dots.empty()) {
                            // 删除最后一个点（即今天的点）
                            int lastQ = dots.back().quadrant;
                            dots.pop_back();
                            if (lastQ >= 0 && lastQ < 4) quadCounts[lastQ]--;
                            WriteDotLog("save/dot_log.txt", quadCounts);
                            RemoveLastHistoryLine("save/dot_history.txt");
                            lastDotDate.clear();
                            SaveAllDataToFile(dots, lastDotDate);
                            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, u8"提示", u8"已删除今天添加的点，可以重新添加。", window);
                        }
                        else {
                            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, u8"提示", u8"今天还没有添加点，无法重置。", window);
                        }
                    }
                }
                // ── 正常加点 ─────────────────
                else {
                    auto t = std::time(nullptr);
                    std::tm tm;
                    localtime_s(&tm, &t);
                    char todayStr[11];
                    std::strftime(todayStr, sizeof(todayStr), "%Y-%m-%d", &tm);
                    std::string today(todayStr);

                    if (dailyLimitEnabled && lastDotDate == today) {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, u8"提示", u8"今天已经添加过点了，请先重置再添加。", window);
                        continue;
                    }

                    int quad = -1;
                    float cx = WINDOW_WIDTH / 2.0f, cy = WINDOW_HEIGHT / 2.0f;
                    if (mx < cx && my < cy) quad = 0;
                    else if (mx >= cx && my < cy) quad = 1;
                    else if (mx < cx && my >= cy) quad = 2;
                    else quad = 3;

                    dots.push_back({ mx, my, quad });
                    quadCounts[quad]++;

                    std::string nowTime = GetCurrentTimeString();
                    LogDotHistory("save/dot_history.txt", quadrantTextsRaw[quad], nowTime);

                    if (dailyLimitEnabled) {
                        lastDotDate = today;
                    }

                    SaveAllDataToFile(dots, lastDotDate);
                    WriteDotLog("save/dot_log.txt", quadCounts);
                    showSuccessMsg = true;
                }
            }
        }

        // 时间更新
        std::string nowTime = GetCurrentTimeString();
        if (nowTime != lastTimeStr) {
            if (timeTextObj) TTF_DestroyText(timeTextObj);
            timeTextObj = TTF_CreateText(textEngine, font, nowTime.c_str(), nowTime.length());
            lastTimeStr = nowTime;
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        float cx = WINDOW_WIDTH / 2.0f, cy = WINDOW_HEIGHT / 2.0f;

        if (timeTextObj) {
            int w, h;
            TTF_GetTextSize(timeTextObj, &w, &h);
            float x = (WINDOW_WIDTH - w) / 2.0f, y = 20.0f;
            TTF_SetTextColor(timeTextObj, 0, 0, 0, 255);
            TTF_DrawRendererText(timeTextObj, x, y);
        }

        // 四象限
        const float line_width = 4.0f, half_width = line_width * 0.5f;
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_FRect vline = { cx - half_width, cy - 0.3f * WINDOW_HEIGHT, line_width, 0.7f * WINDOW_HEIGHT };
        SDL_RenderFillRect(renderer, &vline);
        SDL_FRect hline = { cx - 0.3f * WINDOW_WIDTH, cy - half_width, 0.6f * WINDOW_WIDTH, line_width };
        SDL_RenderFillRect(renderer, &hline);

        float cross_left = cx - CROSS_MARGIN_RATIO * WINDOW_WIDTH;
        float cross_right = cross_left + HORIZONTAL_WIDTH_RATIO * WINDOW_WIDTH;
        float cross_top = cy - CROSS_MARGIN_RATIO * WINDOW_HEIGHT;
        float cross_bottom = cross_top + VERTICAL_HEIGHT_RATIO * WINDOW_HEIGHT;
        float quadX[4] = { (cross_left + cx) / 2, (cx + cross_right) / 2, (cross_left + cx) / 2, (cx + cross_right) / 2 };
        float quadY[4] = { (cross_top + cy) / 2, (cross_top + cy) / 2, (cy + cross_bottom) / 2, (cy + cross_bottom) / 2 };

        for (int i = 0; i < 4; ++i) {
            if (quadrantTexts[i]) {
                int w, h;
                TTF_GetTextSize(quadrantTexts[i], &w, &h);
                float x = quadX[i] - w / 2, y = quadY[i] - h / 2;
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_FRect bg = { x - 4, y - 2, w + 8.0f, h + 4.0f };
                SDL_RenderFillRect(renderer, &bg);
                static const SDL_Color gray = { 128, 128, 128, 255 };
                TTF_SetTextColor(quadrantTexts[i], gray.r, gray.g, gray.b, gray.a);
                TTF_DrawRendererText(quadrantTexts[i], x, y);
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
        for (const auto& dot : dots) {
            SDL_FRect rect = { dot.x - 5, dot.y - 5, 10, 10 };
            SDL_RenderFillRect(renderer, &rect);
        }

        // 删除按钮
        SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
        SDL_RenderFillRect(renderer, &clearBtnRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderRect(renderer, &clearBtnRect);
        if (clearBtnTextObj) {
            int w, h;
            TTF_GetTextSize(clearBtnTextObj, &w, &h);
            float tx = clearBtnRect.x + (clearBtnRect.w - w) / 2, ty = clearBtnRect.y + (clearBtnRect.h - h) / 2;
            TTF_SetTextColor(clearBtnTextObj, 255, 255, 255, 255);
            TTF_DrawRendererText(clearBtnTextObj, tx, ty);
        }

        // 重置按钮
        SDL_SetRenderDrawColor(renderer, 0, 120, 215, 255);
        SDL_RenderFillRect(renderer, &resetTodayBtnRect);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderRect(renderer, &resetTodayBtnRect);
        if (resetTodayBtnTextObj) {
            int w, h;
            TTF_GetTextSize(resetTodayBtnTextObj, &w, &h);
            float tx = resetTodayBtnRect.x + (resetTodayBtnRect.w - w) / 2, ty = resetTodayBtnRect.y + (resetTodayBtnRect.h - h) / 2;
            TTF_SetTextColor(resetTodayBtnTextObj, 255, 255, 255, 255);
            TTF_DrawRendererText(resetTodayBtnTextObj, tx, ty);
        }

        SDL_RenderPresent(renderer);
        if (showSuccessMsg) {
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, u8"提示", u8"添加成功！", window);
            showSuccessMsg = false;
        }
    }

    // 清理资源
    if (timeTextObj) TTF_DestroyText(timeTextObj);
    for (int i = 0; i < 4; ++i) if (quadrantTexts[i]) TTF_DestroyText(quadrantTexts[i]);
    if (centerTextObj) TTF_DestroyText(centerTextObj);
    if (clearBtnTextObj) TTF_DestroyText(clearBtnTextObj);
    if (resetTodayBtnTextObj) TTF_DestroyText(resetTodayBtnTextObj);
    TTF_DestroyRendererTextEngine(textEngine);
    if (font) TTF_CloseFont(font);
    if (smallFont) TTF_CloseFont(smallFont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    quit();
    return 0;
}