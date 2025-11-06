#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <ostream>

/**
 * @brief 错误报告器类
 * 负责收集、排序和打印所有编译错误
 */
class ErrorReporter {
public:
    /**
     * @brief 添加一个错误
     * @param line 行号
     * @param type 错误类型
     */
    void addError(int line, const std::string& type);

    /**
     * @brief 打印所有错误（按行号排序）
     * @param outputStream 输出流，默认为std::cerr
     */
    void printErrors(std::ostream& outputStream = std::cerr) const;

    /**
     * @brief 检查是否有错误
     * @return 如果有错误返回true
     */
    bool hasErrors() const;

    /**
     * @brief 获取错误数量
     * @return 错误数量
     */
    size_t getErrorCount() const;

    /**
     * @brief 清空所有错误
     */
    void clearErrors();

private:
    struct ErrorInfo {
        int line;
        std::string type;

        ErrorInfo(int l, const std::string& t) : line(l), type(t) {}

        bool operator<(const ErrorInfo& other) const {
            return line < other.line;
        }
    };

    std::vector<ErrorInfo> errors;
};