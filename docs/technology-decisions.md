# 기반 기술 선택

2026-10-28 후속 구현: 자동 대국 연결은 선택적 C ABI DLL + Python 표준 ctypes/JSON을 선정했다. 추가 pip 패키지 없이 동일 프로세스에서 C++ Controller를 재사용하며 기본 빌드는 Python을 요구하지 않는다. 프레임워크/추론 도구 선정과는 별개다. [실제 API·검증·측정](python-headless.md)을 따른다.

2026-10-28 목표 변경: 아래 C++20/Win32/CMake 선택은 기존 코어/GUI의 기술 결정으로 유지한다. 학습 언어까지 C++로 강제하지 않는다. Python/학습 프레임워크/추론 런타임·내보내기·장비는 [학습/평가 원칙](learning-and-evaluation.md)에 따라 성능/연동/배포/라이선스를 검토할 미정 사항이다. 이번에 의존성을 선정·설치하거나 기본 빌드를 변경하지 않았다.

같은 날 후속 원칙: 정책·가치 신경망 지도학습 → MCTS → 자기 대국 강화학습을 기본 경로로 두고 AI 자체는 최소 구조/실험으로 발전시킨다. AI 외 모든 기능의 설계/변경은 [계획의 확장성 기준](development-plan.md)에 따라 책임·교체 경계·프로필/형식/공개 인터페이스 버전·관련 회귀를 명시한다. GUI/OS/특정 엔진 의존성은 경계에 모으되 미승인 기능·복잡한 플러그인을 선행 구현하거나 이 원칙만으로 기존 코드를 대규모 재작성하지 않는다.

결정일: 2026-10-19. 사용자에게 승인받은 일반 기술 선택 재량 안에서 결정했다.

| 선택 | 근거 | 비용·제약 |
|---|---|---|
| C++20, 확장 비활성 | 설치된 GCC 13.3.0에서 사용 가능. 향후 상태·수명 관리·비동기 구현에 충분하며 C++23 의존 불필요 | 플랫폼 독립 로직에 Windows 헤더를 넣지 않음 |
| Win32 User32/GDI32 | Windows 전용 요구에 맞는 OS 기본 GUI 라이브러리. 별도 GUI SDK·DLL 설치 없이 창·입력·그리기 가능 | 네이티브 레이아웃·DPI·접근성 처리는 직접 구현해야 함. 이 선택이 규칙 로직을 GUI에 넣을 근거는 아님 |
| CMake 3.24 이상 | 모듈별 타깃, Ninja·MSVC 생성기, 설치·ZIP 패키징을 한 구성에서 관리 | 빌드 도구를 사용자 실행 의존성으로 배포하지 않음 |
| Ninja + 기존 MinGW-w64 x64 | 이미 설치돼 있어 추가 설치 없이 실제 빌드 가능. GCC 런타임 정적 링크로 배포 파일 수 감소 | 설치된 Ninja/windres의 한글·공백 경로 문제를 scripts/build.ps1의 ASCII 경로 별칭과 상대 리소스 컴파일로 회피 |
| MSVC 2022 선택 경로 | 향후 Windows SDK 도구와 표준적인 Windows 빌드 비교 가능 | 현재 설치 확인되지 않았고 빌드 미검증. 기본 경로로 요구하지 않음 |

Qt·wxWidgets도 후보가 될 수 있지만 현재 Windows 전용 최소 창과 향후 바둑판에 필수는 아니다. Win32를 선택해 GUI 런타임 배포와 프레임워크 라이선스 관리 부담을 줄였다. GUI 교체가 필요해져도 도메인·대국 제어·통신에는 영향을 주지 않는 경계를 유지한다.

## 라이선스와 배포
- OS의 User32/GDI32는 Windows 제공 DLL을 링크해 사용한다. DLL을 복사해 배포하지 않는다. Microsoft SDK의 코드·샘플을 복사해 제품에 포함하지 않았고, 현재 빌드는 MinGW-w64 헤더·import library를 사용한다.
- MinGW-w64 기본 ZPL 2.1과 파일별 예외, GCC GPL+Runtime Library Exception 3.1을 공식 원문으로 확인했다. 고지와 라이선스 원문을 패키지에 포함한다. 상세 내용은 [배포 고지](../THIRD_PARTY_NOTICES.md).
- CMake BSD 계열, Ninja Apache-2.0은 빌드 도구의 조건이다. 실행 패키지는 해당 도구를 포함하지 않는다. 설치된 CMake 3.29.3 Copyright 원문을 별도 조사 캐시에서 확인했다.
- 프로젝트 자체 공개 라이선스는 선정하지 않았다. 이 결정은 사용자에게 프로젝트 전체 라이선스를 임의로 부여하는 조치가 아니다.

## 공식 참고
- [Microsoft의 기본 Win32 창·메시지 루프](https://learn.microsoft.com/en-us/windows/win32/learnwin32/your-first-windows-program)
- [GCC C++ 표준 지원](https://gcc.gnu.org/projects/cxx-status.html)
- [CMake 3.29.3 Copyright](https://github.com/Kitware/CMake/blob/v3.29.3/Copyright.txt)
- [Ninja 1.12.1 COPYING](https://github.com/ninja-build/ninja/blob/v1.12.1/COPYING)
- [MinGW-w64 11.0.1 COPYING](https://github.com/mingw-w64/mingw-w64/blob/v11.0.1/COPYING)
- [GCC 런타임 라이선스](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html)

## 빌드 경로 문제와 대응
기존 Ninja 1.12.1은 이 폴더의 한글 경로를 포함한 의존성 조회에서 FindFirstFileExA 오류가 났고, windres는 공백이 있는 절대 include 경로를 잘못 분리했다. 리소스 파일은 독립적인 상대 파일명으로 컴파일하고, 빌드 스크립트는 ASCII 임시 경로에 현재 프로젝트를 가리키는 junction만 만든다. 소스를 이동·복사하지 않는다. 이미 다른 대상을 가리키는 alias가 있으면 삭제하지 않고 오류로 종료한다. 이전 실패 캐시와 새 빌드 디렉터리를 분리했다.

## 상태 모델·테스트 추가

- 상태는 C++20 정적 라이브러리이며 보드와 최대 225개 배치 이력을 고정 배열로 소유한다. 쓰기 전 검증과 비할당 갱신으로 거부 시 상태 보존을 단순하게 한다. GUI/Windows 링크는 추가하지 않는다.
- 참가자·돌 색·빈칸·자신/상대는 서로 다른 타입으로 구분한다. 색 소유자와 실제 배치 참가자를 분리하고, 결과는 variant로 표현해 무승부에 승자를 붙이는 모양을 방지한다.
- 외부 테스트 프레임워크 없이 CTest와 작은 CHECK 기반 실행기를 사용한다. Release의 NDEBUG로 검사가 제거되지 않으며 의존성 다운로드·새 라이선스 부담이 없다. 자동 테스트 13개를 개별 CTest 사례로 등록한다.
- 기존 빌드 스크립트의 경로 우회·설치 동작은 변경하지 않는다. 기본 CMake 빌드가 테스트 실행 파일까지 만들고 CTest는 README에 기록한 별도 명령으로 실행한다.

## 로컬 대국과 금수 작업자 추가 (2026-10-24)

match는 표준 C++20 정적 라이브러리, 화면 좌표는 Windows 독립 omok_geometry로 분리했다. 금수 계산은 한 std::thread의 최신 요청 합치기·스냅샷·세션/revision 검증을 사용한다. UI 잠금과 이전 대국 결과 적용을 막기 위한 선택이다. 기존 규칙 재귀를 자르지 않으므로 현재 후보 완료까지의 종료 지연은 상한 미검증이다. 현재 POSIX MinGW의 winpthreads는 정적 포함되며 고지를 licenses에 추가했다. 세부 계약과 실제 증거는 [match 계약](match-contract.md)·[검증](verification-match.md)을 따른다.

## Rapfi 프로세스 어댑터 (2026-10-24)

Win32 CreateProcessW·제한 핸들 상속·파이프·Job·단일 작업자를 사용해 GUI와 분리했다. 첫 연결은 매 수 새 프로세스와 순서 보존 BOARD를 사용해 상태 오염을 막지만 TT/가중치 캐시를 재사용하지 않는다. CPU는 CPUID/OSXSAVE/XGETBV로 검사하고 AVX2/SSE만 자동 선택한다. 자체AI/제3자 소스 편입 없이 사용자 설치 엔진을 실행한다. 엔진 초기화/지연 로드 비용·메모리 예산·취소 정책의 실제 근거와 제약은 [Rapfi 계약](rapfi-integration.md)·[검증](verification-rapfi.md)에 기록했다.
