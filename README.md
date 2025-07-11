# ray-scene-tracer

`ray-scene-tracer`는 `.rt` 장면을 읽어 P3 PPM 이미지를 만드는 C++17 CPU ray tracer를 단계적으로 구현하는 프로젝트다.

## 초기 개발 규약

- C++17과 표준 라이브러리를 기준으로 한다.
- 직접 빌드는 `-Wall -Wextra -Wpedantic` 경고를 활성화한다.
- 소스, 공개 선언, 장면 fixture, 검사를 `src/`, `include/`, `scenes/`, `tests/`에 분리한다.
- 성능 근거가 생기면 측정 코드와 결과를 `benchmarks/`에 둔다.
- 실행 파일, object, build directory, 생성 PPM은 버전 관리하지 않는다.
- 구현 커밋은 해당 시점에 가능한 build와 결정적 검증을 통과시킨다.
- 실패 계약과 자원 소유권은 공개 경계와 함께 관리한다.

## 예정 범위

- 해상도, 환경광, 카메라, 점광원 장면 입력
- 구, 평면, 유한 원기둥 교차 계산
- 픽셀 중심 광선, 직접광, 그림자, P3 PPM 출력
- 선형 기준 구현을 보존하는 가속과 실행 모드 간 결과 결정성

GUI, GPU 렌더링, mesh, texture, 장면 편집기는 초기 범위에 포함하지 않는다.
