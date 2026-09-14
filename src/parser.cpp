#include "ray/parser.hpp"

#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace ray {

namespace {

std::string makeParseMessage(const std::string& source_name,
                             std::size_t line_number,
                             const std::string& message) {
    std::ostringstream output;
    output << source_name;
    if (line_number > 0) {
        output << ':' << line_number;
    }
    output << ": " << message;
    return output.str();
}

std::string trim(const std::string& value) {
    std::size_t first = 0;
    while (first < value.size() &&
           std::isspace(static_cast<unsigned char>(value[first]))) {
        ++first;
    }

    std::size_t last = value.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(value[last - 1]))) {
        --last;
    }
    return value.substr(first, last - first);
}

std::vector<std::string> splitTokens(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream input(line);
    std::string token;
    while (input >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

std::vector<std::string> splitCommas(const std::string& token) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= token.size()) {
        const std::size_t comma = token.find(',', start);
        if (comma == std::string::npos) {
            parts.push_back(token.substr(start));
            break;
        }
        parts.push_back(token.substr(start, comma - start));
        start = comma + 1;
    }
    return parts;
}

void expectCount(const std::vector<std::string>& tokens,
                 std::size_t expected,
                 const std::string& source_name,
                 std::size_t line_number,
                 const std::string& form) {
    if (tokens.size() != expected) {
        std::ostringstream message;
        message << "expected " << form << " with " << expected - 1
                << " argument(s), got " << tokens.size() - 1;
        throw ParseError(source_name, line_number, message.str());
    }
}

void expectCountEither(const std::vector<std::string>& tokens,
                       std::size_t first,
                       std::size_t second,
                       const std::string& source_name,
                       std::size_t line_number,
                       const std::string& form) {
    if (tokens.size() != first && tokens.size() != second) {
        std::ostringstream message;
        message << "expected " << form << " with "
                << first - 1 << " or " << second - 1
                << " argument(s), got " << tokens.size() - 1;
        throw ParseError(source_name, line_number, message.str());
    }
}

// [INTV:EDGE] std::stod는 "12abc" 같은 입력도 앞부분(12)만 파싱하고 성공 처리한다 — parsed 출력
// 파라미터로 실제 소비한 문자 수를 받아 token.size()와 비교해야 "꼬리에 쓰레기가 붙은" 입력을 잡아낼
// 수 있다. std::isfinite 검사는 "inf"/"nan" 문자열이 유효한 double로 파싱되는 것까지 추가로 거부한다.
// - [TRAP] parsed 체크 없이 stod의 반환값만 쓰면, "1.5garbage" 같은 입력이 조용히 1.5로 통과해버려
//   오타가 섞인 장면 파일이 에러 없이 잘못된 값으로 렌더링되는 버그가 된다.
double parseDoubleToken(const std::string& token,
                        const std::string& source_name,
                        std::size_t line_number,
                        const std::string& field_name) {
    try {
        std::size_t parsed = 0;
        const double value = std::stod(token, &parsed);
        if (parsed != token.size() || !std::isfinite(value)) {
            throw std::invalid_argument("not a finite number");
        }
        return value;
    } catch (const std::exception&) {
        throw ParseError(source_name,
                         line_number,
                         "invalid " + field_name + " value '" + token + "'");
    }
}

int parseIntToken(const std::string& token,
                  const std::string& source_name,
                  std::size_t line_number,
                  const std::string& field_name) {
    try {
        std::size_t parsed = 0;
        const long long value = std::stoll(token, &parsed);
        if (parsed != token.size() ||
            value < 1 ||
            value > std::numeric_limits<int>::max()) {
            throw std::invalid_argument("not a positive int");
        }
        return static_cast<int>(value);
    } catch (const std::exception&) {
        throw ParseError(source_name,
                         line_number,
                         "invalid " + field_name + " value '" + token + "'");
    }
}

double parseRatio(const std::string& token,
                  const std::string& source_name,
                  std::size_t line_number,
                  const std::string& field_name) {
    const double value =
        parseDoubleToken(token, source_name, line_number, field_name);
    if (value < 0.0 || value > 1.0) {
        throw ParseError(source_name,
                         line_number,
                         field_name + " must be between 0.0 and 1.0");
    }
    return value;
}

double parsePositiveDouble(const std::string& token,
                           const std::string& source_name,
                           std::size_t line_number,
                           const std::string& field_name) {
    const double value =
        parseDoubleToken(token, source_name, line_number, field_name);
    if (value <= 0.0) {
        throw ParseError(source_name,
                         line_number,
                         field_name + " must be positive");
    }
    return value;
}

Vec3 parseVec3(const std::string& token,
               const std::string& source_name,
               std::size_t line_number,
               const std::string& field_name) {
    const std::vector<std::string> parts = splitCommas(token);
    if (parts.size() != 3 ||
        parts[0].empty() ||
        parts[1].empty() ||
        parts[2].empty()) {
        throw ParseError(source_name,
                         line_number,
                         field_name + " must use x,y,z format");
    }
    return Vec3(
        parseDoubleToken(parts[0],
                         source_name,
                         line_number,
                         field_name + ".x"),
        parseDoubleToken(parts[1],
                         source_name,
                         line_number,
                         field_name + ".y"),
        parseDoubleToken(parts[2],
                         source_name,
                         line_number,
                         field_name + ".z"));
}

Color parseColor(const std::string& token,
                 const std::string& source_name,
                 std::size_t line_number,
                 const std::string& field_name) {
    const std::vector<std::string> parts = splitCommas(token);
    if (parts.size() != 3 ||
        parts[0].empty() ||
        parts[1].empty() ||
        parts[2].empty()) {
        throw ParseError(source_name,
                         line_number,
                         field_name + " must use r,g,b format");
    }

    int channels[3] = {};
    for (std::size_t index = 0; index < 3; ++index) {
        try {
            std::size_t parsed = 0;
            const long long value = std::stoll(parts[index], &parsed);
            if (parsed != parts[index].size() ||
                value < 0 ||
                value > 255) {
                throw std::invalid_argument("not a byte");
            }
            channels[index] = static_cast<int>(value);
        } catch (const std::exception&) {
            throw ParseError(source_name,
                             line_number,
                             "invalid " + field_name + " channel '" +
                                 parts[index] + "'");
        }
    }
    return Color(static_cast<double>(channels[0]) / 255.0,
                 static_cast<double>(channels[1]) / 255.0,
                 static_cast<double>(channels[2]) / 255.0);
}

// [INTV:ARCH] 마지막 인자(material)가 선택적이라는 것을 토큰 개수(tokens.size() == base_count)로
// 판별 — expectCountEither가 "필수 개수 또는 필수+1 개수"만 통과시켜주므로, 여기서는 "+1이 실제로
// 왔는가"만 보면 된다(값 자체의 형식은 아래에서 diffuse/metal 문자열 매칭으로 검증).
MaterialType parseMaterialType(const std::vector<std::string>& tokens,
                               std::size_t base_count,
                               const std::string& source_name,
                               std::size_t line_number) {
    if (tokens.size() == base_count) {
        return MaterialType::Diffuse;
    }
    const std::string& token = tokens.back();
    if (token == "diffuse") {
        return MaterialType::Diffuse;
    }
    if (token == "metal") {
        return MaterialType::Metal;
    }
    throw ParseError(source_name,
                     line_number,
                     "unknown material '" + token + "'");
}

void rejectDuplicate(bool already_seen,
                     const std::string& source_name,
                     std::size_t line_number,
                     const std::string& directive) {
    if (already_seen) {
        throw ParseError(source_name,
                         line_number,
                         "duplicate " + directive + " directive");
    }
}

// [INTV:EDGE] 카메라 방향/평면 법선/실린더 축처럼 "방향"을 나타내는 벡터가 (0,0,0)으로 들어오면
// normalize()가 (math.cpp의 가드에 의해) 조용히 영벡터를 반환해 이후 좌표계/교차 계산이 전부 무의미해
// 진다 — 파싱 시점에 미리 거부해 그 조용한 실패가 렌더링 단계까지 전파되지 않게 한다.
void requireNonzeroVector(const Vec3& value,
                          const std::string& source_name,
                          std::size_t line_number,
                          const std::string& field_name) {
    if (value.length() <= kEpsilon) {
        throw ParseError(source_name,
                         line_number,
                         field_name + " must not be the zero vector");
    }
}

}  // namespace

ParseError::ParseError(const std::string& source_name,
                       std::size_t line_number,
                       const std::string& message)
    : std::runtime_error(
          makeParseMessage(source_name, line_number, message)),
      source_(source_name),
      line_(line_number) {}

const std::string& ParseError::source() const noexcept {
    return source_;
}

std::size_t ParseError::line() const noexcept {
    return line_;
}

namespace parser {

// [INTV:ARCH] .rt 장면 파일 형식의 재귀 하향(recursive descent 없는 단순 라인 기반) 파서 — 각 줄을
// 독립적으로 토큰화한 뒤 첫 토큰(directive id)으로 분기하는 디스패치 테이블 구조. R/A/C는 정확히 한
// 번만 나와야 하는 필수 설정(rejectDuplicate로 강제), L/sp/pl/cy는 여러 번 나올 수 있는 항목이다.
// - [FLOW] 1. 줄 단위로 읽어 '#' 이후 주석 제거 -> 2. trim 후 빈 줄 스킵 -> 3. 공백 기준 토큰화 ->
//   4. 첫 토큰으로 디렉티브 분기, 각 분기가 정확한 인자 개수/형식/범위를 검증 후 Scene에 반영 ->
//   5. 파일 전체를 다 읽은 뒤 필수 디렉티브(R/A/C) 존재 여부를 최종 확인 -> 6. buildAcceleration()
// - [TRAP] 필수 디렉티브 체크를 루프 "안"에서(각 줄마다) 하지 않고 루프가 끝난 뒤 한 번만 하는 이유가
//   중요하다 — R/A/C가 파일의 어느 순서에 와도(예: C가 R보다 먼저 나와도) 파싱이 끝나야 비로소
//   전체 구성이 완전한지 판단할 수 있기 때문이다. 각 줄 처리 시점에 "아직 안 나왔다고 에러"를 내면
//   정상적인 순서 조합 상당수를 잘못 거부하게 된다.
Scene parseScene(std::istream& input, const std::string& source_name) {
    Scene scene;
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        const std::size_t comment = line.find('#');
        if (comment != std::string::npos) {
            line.erase(comment);
        }
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        const std::vector<std::string> tokens = splitTokens(line);
        const std::string& id = tokens[0];

        if (id == "R") {
            expectCount(tokens,
                        3,
                        source_name,
                        line_number,
                        "R width height");
            rejectDuplicate(scene.hasResolution,
                            source_name,
                            line_number,
                            "R");
            scene.width =
                parseIntToken(tokens[1],
                              source_name,
                              line_number,
                              "width");
            scene.height =
                parseIntToken(tokens[2],
                              source_name,
                              line_number,
                              "height");
            scene.hasResolution = true;
        } else if (id == "A") {
            expectCount(tokens,
                        3,
                        source_name,
                        line_number,
                        "A ratio r,g,b");
            rejectDuplicate(scene.hasAmbient,
                            source_name,
                            line_number,
                            "A");
            scene.ambientRatio =
                parseRatio(tokens[1],
                           source_name,
                           line_number,
                           "ambient ratio");
            scene.ambientColor =
                parseColor(tokens[2],
                           source_name,
                           line_number,
                           "ambient color");
            scene.hasAmbient = true;
        } else if (id == "C") {
            expectCount(tokens,
                        4,
                        source_name,
                        line_number,
                        "C pos dir fov");
            rejectDuplicate(scene.hasCamera,
                            source_name,
                            line_number,
                            "C");
            const Vec3 position =
                parseVec3(tokens[1],
                          source_name,
                          line_number,
                          "camera position");
            const Vec3 direction =
                parseVec3(tokens[2],
                          source_name,
                          line_number,
                          "camera direction");
            requireNonzeroVector(direction,
                                 source_name,
                                 line_number,
                                 "camera direction");
            const double fov =
                parseDoubleToken(tokens[3],
                                 source_name,
                                 line_number,
                                 "camera fov");
            if (fov <= 0.0 || fov >= 180.0) {
                throw ParseError(
                    source_name,
                    line_number,
                    "camera fov must be greater than 0 and less than 180");
            }
            scene.camera = Camera(position, normalize(direction), fov);
            scene.hasCamera = true;
        } else if (id == "L") {
            expectCount(tokens,
                        4,
                        source_name,
                        line_number,
                        "L pos brightness r,g,b");
            const Vec3 position =
                parseVec3(tokens[1],
                          source_name,
                          line_number,
                          "light position");
            const double brightness =
                parseRatio(tokens[2],
                           source_name,
                           line_number,
                           "light brightness");
            const Color color =
                parseColor(tokens[3],
                           source_name,
                           line_number,
                           "light color");
            scene.addLight(Light(position, brightness, color));
        } else if (id == "sp") {
            expectCountEither(tokens,
                              4,
                              5,
                              source_name,
                              line_number,
                              "sp center diameter r,g,b [material]");
            const Vec3 center =
                parseVec3(tokens[1],
                          source_name,
                          line_number,
                          "sphere center");
            const double diameter =
                parsePositiveDouble(tokens[2],
                                    source_name,
                                    line_number,
                                    "sphere diameter");
            const Material material(
                parseColor(tokens[3],
                           source_name,
                           line_number,
                           "sphere color"),
                parseMaterialType(
                    tokens, 4, source_name, line_number));
            scene.addShape(std::make_unique<Sphere>(
                center,
                diameter * 0.5,
                material));
        } else if (id == "pl") {
            expectCountEither(tokens,
                              4,
                              5,
                              source_name,
                              line_number,
                              "pl point normal r,g,b [material]");
            const Vec3 point =
                parseVec3(tokens[1],
                          source_name,
                          line_number,
                          "plane point");
            const Vec3 normal =
                parseVec3(tokens[2],
                          source_name,
                          line_number,
                          "plane normal");
            requireNonzeroVector(normal,
                                 source_name,
                                 line_number,
                                 "plane normal");
            const Material material(
                parseColor(tokens[3],
                           source_name,
                           line_number,
                           "plane color"),
                parseMaterialType(
                    tokens, 4, source_name, line_number));
            scene.addShape(
                std::make_unique<Plane>(point, normal, material));
        } else if (id == "cy") {
            expectCountEither(
                tokens,
                6,
                7,
                source_name,
                line_number,
                "cy center axis diameter height r,g,b [material]");
            const Vec3 center =
                parseVec3(tokens[1],
                          source_name,
                          line_number,
                          "cylinder center");
            const Vec3 axis =
                parseVec3(tokens[2],
                          source_name,
                          line_number,
                          "cylinder axis");
            requireNonzeroVector(axis,
                                 source_name,
                                 line_number,
                                 "cylinder axis");
            const double diameter =
                parsePositiveDouble(tokens[3],
                                    source_name,
                                    line_number,
                                    "cylinder diameter");
            const double height =
                parsePositiveDouble(tokens[4],
                                    source_name,
                                    line_number,
                                    "cylinder height");
            const Material material(
                parseColor(tokens[5],
                           source_name,
                           line_number,
                           "cylinder color"),
                parseMaterialType(
                    tokens, 6, source_name, line_number));
            scene.addShape(std::make_unique<Cylinder>(
                center,
                axis,
                diameter * 0.5,
                height,
                material));
        } else {
            throw ParseError(source_name,
                             line_number,
                             "unknown directive '" + id + "'");
        }
    }

    if (!scene.hasResolution) {
        throw ParseError(
            source_name, 0, "missing R width height directive");
    }
    if (!scene.hasAmbient) {
        throw ParseError(
            source_name, 0, "missing A ratio r,g,b directive");
    }
    if (!scene.hasCamera) {
        throw ParseError(
            source_name, 0, "missing C pos dir fov directive");
    }
    scene.buildAcceleration();
    return scene;
}

Scene parseSceneText(const std::string& text,
                     const std::string& source_name) {
    std::istringstream input(text);
    return parseScene(input, source_name);
}

Scene parseSceneFile(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw ParseError(path, 0, "unable to open scene file");
    }
    return parseScene(input, path);
}

}  // namespace parser

Scene loadScene(const std::string& path) {
    return parser::parseSceneFile(path);
}

}  // namespace ray
