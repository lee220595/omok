# 오목 프로젝트 (omok)

최종 목표는 **지도학습과 강화학습을 기반으로 강력한 자체 오목 AI를 개발하고, Gomocup에서 실행·대국할 독립 엔진으로 완성하는 것**이다(2026-10-28 확정). 주 산출물은 자체 학습 모델과 탐색을 사용하는 제출 엔진이다. 상태·규칙·오프닝·대국 관리기는 학습/자동 대국/평가 기반, GUI는 관찰·오류 분석을 위한 보조 도구다.

개발 순서는 규칙 정확성 → 자동 대국·데이터 기반 → 지도학습 → 탐색 결합 → 자기 대국 강화학습 → 성능 평가·제출 검증이다. 자체 엔진의 프로토콜 수신은 초기에 최소 동작을 검증하고 회귀한다. GUI 편의 기능 전체나 클린 GUI 배포 검증을 학습 착수의 선행 조건으로 두지 않는다. [개발 계획과 다음 작업 하나](docs/development-plan.md)·[학습/데이터/평가 원칙](docs/learning-and-evaluation.md)·[승인/미정 사항](docs/requirements-pending.md)을 따른다.

**현재 구현**은 Windows10·11 x64 C++20 기반15×15 렌주 상태/규칙/오프닝/match, 로컬 사람 대 사람·사람 대 Rapfi GUI와 준비/빈 보드 시작·기권/재시작·금수 표시다. [처음부터 대국 검증](docs/verification-direct-start.md)과 [Rapfi 검증](docs/verification-rapfi.md)은 과거 실행 근거이며 전체 대회/장기 대국 검증은 아니다. Python 무화면 환경·순차 자동 대국·JSON 기록/재생/재개는 구현했다([설치·API·검증](docs/python-headless.md)). 학습/모델/탐색·자체 제출 엔진 수신은 아직 없다. 기존 src/ai는 외부 엔진에 명령을 보내는 어댑터이며 자체 엔진이 명령을 받는 구현과 구분한다. 2026-10-28 사용자 승인으로 무화면 자동 대국 기반을 구현했으며 모델/학습은 시작하지 않았다.

현재15×15 렌주는 잠정 기준이고 목표 참가 연도·리그는 미정이다. 당시 일반 대회 프로필과 학습 설정을 분리하며 시간/자원/200수/오프닝 조건을 전 대회·학습 공통 상수로 확정하지 않는다. [당시 조사](docs/compatibility-research.md)를 보존한다. SWAP2BOARD의 기존 별도 요구와 목표 리그의 필수 여부도 구분한다. 공식 대상 확인 전 제출 적합성을 주장하거나 실제 등록/서명/업로드하지 않는다.

학습 환경은 데이터 처리·학습·자기 대국·평가, 제출 엔진은 모델 로딩·추론·탐색·Gomocup 응답을 맡는다. 기본 AI 경로는 **정책·가치 신경망의 지도학습 → MCTS 탐색 결합 → 자기 대국 강화학습**이다. C++20 기반은 유지하되 학습에는 Python 등 별도 언어를 사용할 수 있다. 모델 크기/세부 구조·MCTS 설정·프레임워크·추론 도구·장비는 검증으로 정하고, 최소 구현에서 실험/성능 측정에 따라 발전시킨다. 학습 의존성을 아래 기존 C++/GUI 빌드에 강제하지 않는다. 아래 산출물은 현재 GUI/검증 도구이며 학습된 독립 엔진이 아니다.

Rapfi는 **중간·최종 평가용 고정 외부 벤치마크**다. 현재 계획에서 학습 상대나 정답 생성 교사로 사용하지 않는다. 이전 자체 모델·단순 기준 플레이어·Rapfi 평가를 구분하고 같은 시작 배치에서 흑백을 교대한다. 학습/검증/평가 데이터와 버전·설정·시드·배치·자원 조건을 기록하며 학습 성공·실력 향상·프로토콜 호환성·대회 준수를 별도로 검증한다.

설계된 공통 계약은 [모델 입출력·SL/RL 연결·체크포인트](docs/model-contract.md), [무화면 환경·종료/중단·보상](docs/headless-environment-contract.md), [데이터 계보·정답 자격·분할](docs/data-contract.md), [개발/최종 평가·점수·불확실성](docs/evaluation-contract.md)에 정의한다. 무화면 환경과 최소 기록/재생은 구현했으며 학습/모델·전체 평가 기능은 미구현이다. 같은 단계 재개와 SL→RL 가중치 초기화를 구분하고, 실제 금수패를 ‘실패 요청 상태 보존’이나 학습 마스크 처리와 혼동하지 않는다. 다음 작업은 작은 정책·가치 모델의 지도학습과 추론 연결이며 자동 착수하지 않는다.

AI 외 기능은 책임/인터페이스와 버전 있는 데이터·프로필을 통해 확장/교체에 대비한다. GUI/OS/특정 엔진 의존성은 경계에 두고 관련 회귀로 기존 동작을 보호한다. 미승인 기능을 선행 구현하거나 이 원칙만으로 플러그인 구조·대규모 재작성을 도입하지 않는다. 상세 대상과 기준은 [개발 계획](docs/development-plan.md)을 따른다.

## 준비와 확인
현재 컴퓨터에서는 다음 도구를 확인했다. 추가 설치 없이 MinGW 빌드 가능하다.

| 도구 | 확인한 버전·위치 |
|---|---|
| GCC/MinGW-w64 | g++ 13.3.0, x86_64-w64-mingw32, MinGW-w64 11.0.1 |
| CMake | 3.29.3 |
| Ninja | 1.12.1 |
| 도구 위치 | C:\Program Files\mingw64\bin |

PowerShell에서 확인한다.

```powershell
cmake --version
ninja --version
g++ --version
g++ -dumpmachine
Get-Command cmake, ninja, g++, windres
```

PATH에 없다면 **현재 PowerShell 세션에만** 다음을 적용한 뒤 재확인한다.

```powershell
$env:Path = 'C:\Program Files\mingw64\bin;' + $env:Path
```

다른 개발 컴퓨터에 도구가 없으면 [MinGW-w64 안내](https://www.mingw-w64.org/downloads/)에서 GCC x64 도구 체인(g++·windres 포함), [CMake 3.24 이상](https://cmake.org/download/), [Ninja](https://github.com/ninja-build/ninja/releases)를 준비하고 각 bin 경로를 개발 세션의 PATH에 추가한다. 컴파일러 타깃 출력은 `x86_64-w64-mingw32`여야 한다. GUI 프레임워크·Python·외부 AI 설치는 현재 빌드에 필요 없다.

## 빌드·실행 — 검증 경로
프로젝트 루트에서 다음 순서로 실행한다.

```powershell
Set-Location -LiteralPath 'C:\Users\leesn\Documents\ChatGPT\오목 프로젝트'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
.\dist\omok\omok.exe
```

`ExecutionPolicy Bypass`는 이 스크립트를 실행하는 자식 PowerShell 프로세스에만 적용한다. 시스템 실행 정책이나 보안 설정을 변경하지 않는다. 스크립트 실행이 이미 허용된 환경은 `& .\scripts\build.ps1`로 실행해도 된다.

스크립트는 도구·x64 타깃을 확인하고, 임시 폴더에 이 프로젝트를 가리키는 `omok-build-<경로 해시>` junction을 만들어 CMake·Ninja에 ASCII 경로를 제공한다. 소스는 이동·복사하지 않는다. 설치된 Ninja/windres의 한글·공백 경로 문제를 피하기 위한 처리다. 별칭의 기존 대상이 다르면 중단하며 파일·폴더를 삭제하지 않는다. 별칭은 후속 빌드 재사용을 위해 남긴다. TEMP 자체에 비ASCII 문자가 있다면 기존의 ASCII 전용 임시 디렉터리를 `-AliasParent`로 지정한다.

```powershell
# 예: C:\Temp가 이미 존재할 때만 사용
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent C:\Temp
```

2026-10-23에는 기존 TEMP 별칭의 대상 불일치로 기본 명령이 중단되어, 기존 연결을 보존하고 다음 명령으로 실제 빌드했다. 같은 문제가 있다면 그대로 사용할 수 있다.

```powershell
$aliasParent = Join-Path $env:TEMP 'omok-rules-build'
New-Item -ItemType Directory -Force $aliasParent | Out-Null
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent $aliasParent
```

산출물:

- `build/native-release/bin/omok.exe`: 빌드 실행 파일
- `build/native-release/bin/omok_state_tests.exe`: CTest가 실행하는 상태 테스트(배포 폴더에는 넣지 않음)
- `build/native-release/bin/omok_rules_tests.exe`: 규칙 회귀 테스트·선택적 심판 비교 입력기(배포 폴더에는 넣지 않음)
- `build/native-release/bin/omok_opening_tests.exe`: 준비 오프닝·Swap2 회귀와 선택적 심판 스냅샷 비교(배포 폴더에는 넣지 않음)
- `build/native-release/bin/omok_match_tests.exe`: match·좌표·금수 캐시/작업자 회귀 및 benchmark(배포 폴더에는 넣지 않음)
- `dist/omok/`: 실행 파일·README·외부 고지·라이선스가 모인 배포 준비 폴더
- 창 제목은 “오목 프로젝트 · 로컬 대국”. 닫기(X)나 Alt+F4로 정상 종료한다.

재빌드·실행 파일 교체 전에는 실행 중인 창을 닫는다. 그렇지 않으면 Windows 파일 잠금으로 링크가 실패할 수 있다. 빌드 스크립트가 실행 중인 앱을 임의 종료하지는 않는다. 스크립트는 이전 실패 경로가 남은 `.ninja_deps`·`.ninja_log` 두 생성 파일만 정리하고 새 CMake 구성을 수행한다.

압축 해제 후 실행 패키지를 만들려면 빌드 완료 후 다음을 실행한다.

```powershell
Compress-Archive -Path .\dist\omok\* -DestinationPath .\dist\omok-0.1.0-windows-x64.zip -Force
```

생성한 ZIP은 수동 대국 검증이 남은 개발용 프로그램이다. 게임 완성본·대회 제출 엔진으로 소개하지 않는다. 현재 MinGW의 GCC·winpthreads 런타임은 정적 링크하고 필요한 고지를 함께 install한다. DLL 검사와 대상 OS 미검증 범위는 [최신 검증](docs/verification-match.md)을 참고한다.

### 로컬 대국 사용법

실제 클릭 검증 결과와 남은 제약은 [착수·차례 전환 재확인](docs/verification-playability.md)에 기록했다.

1. `dist/omok/omok.exe`에서 첫 목록의 **처음부터 · 빈 보드 (렌주)**(초기 선택), 대국 방식 **로컬 사람 대 사람**, 흑 A/백 B 또는 흑 B/백 A를 선택하고 **대국 시작**을 누른다. 0수·흑 차례로 시작한다. 실행 직후에는 시작 대기 화면이므로 먼저 버튼을 눌러야 한다.
2. 보드 교차점을 클릭하면 현재 참가자의 수가 즉시 확정되고 **다음 색·참가자로 자동 교대**한다. 별도 다음 수 버튼은 없다. 오른쪽에서 차례·총 수순·마지막 수·결과를 확인한다. 판 밖/교차점 사이/중복 입력은 거부한다. 가장자리에서는 바깥 격자선의 안쪽을 클릭한다. 흑 금수 클릭은 **백 승리로 종료**하며 재입력하지 않는다.
3. “현재 차례 기권”은 해당 참가자의 패배로 종료한다. **재시작**은 원래 시작 방식과 색 배정으로 돌아간다. 처음부터 대국은 빈 보드·0수·흑 차례로 돌아간다. 종료 후 추가 착수는 막는다.
4. “흑 금수 표시”는 기본 켬이며 흑 차례에만 빈 교차점의 금수를 붉은 X로 표시한다. 계산 중에도 착수를 요청할 수 있고 실제 판정은 최신 상태로 수행한다. 마지막 성공 착수는 붉은 원이다.
5. 준비 배치를 사용하려면 첫 목록의 “개발 확인용” 배치를 선택한다. 이때 compact 입력을 직접 바꿀 수 있으며, 가져오기에 실패하면 진행 중 대국을 보존하고 원인을 표시한다. 처음부터 모드에서는 compact 입력을 비활성화한다. 준비3수 `h8i8g9`로 시작할 때에는 백 차례다.

개발용 준비 배치는 기본3수·승리7수·금수8수·무승부199수·이미 종료된9수다. 모두 자체 작성한 배치이며 공식 오프닝이 아니다. **처음부터 대국은 기존 렌주 보드 판정(흑 금수·정확한5목, 백5목 이상)과200수 종료를 사용하되, 대회용 전문가 준비 오프닝이나 정식 렌주 오프닝 절차를 구현한 모드가 아니다.** 첫 수를 자동 배치하거나 강제하지 않는다. 대회용 PreparedRenju 계약은 별도로 유지한다. 보드 위쪽 여백은240 논리 픽셀이며 과거 검증의 고정 픽셀을 재사용하지 않는다.

### 사람 대 Rapfi

Rapfi는 빌드/배포물에 포함하지 않는다. 사용자가 실행 파일·config.toml·가중치 전체를 `external/rapfi/Rapfi-engine/`에 준비한다. 현재 PC에서는 `pbrain-rapfi-windows-avx2.exe`를 사용한다. GUI가 상위 프로젝트 폴더에서 설치 위치를 찾아 CPU에 맞는 AVX2/SSE 경로를 채우며, 다른 위치라면 “Rapfi 경로”에 실행 파일의 전체 경로를 입력한다. 필요한 파일과 해시는 [설치·계약](docs/rapfi-integration.md)에 있다.

첫 목록은 **처음부터 · 빈 보드 (렌주)**, 색 배정은 **흑 A / 백 B**로 두고 대국 방식을 선택한다.

- **사람 A / Rapfi B**: 사람이 흑 첫 수를 클릭하면 Rapfi가 백 응답을 둔다.
- **Rapfi A / 사람 B**: 시작 직후 Rapfi가 흑 첫 수를 두고 사람이 백으로 응답한다.

**대국 시작**을 누르면 시작하며 흑 B/백 A로 배정을 바꾸면 선후공도 바뀐다. 준비 배치에서 이어 두는 방식도 유지한다. 사람은 자신의 차례에만 클릭할 수 있고 사고 중에도 재시작·사람 기권·닫기를 사용할 수 있다. 통신 로그는 실행 파일 옆 `logs/engine-*.log`에 남긴다. 빈 보드의 엔진 첫 수는 BEGIN, 이후는 실제 수순의 BOARD를 전송한다.

GUI는 AI 한 수 30초·누적 예산 180초, 프로세스 메모리 1GiB·한 논리 CPU를 제한한다. 엔진에는 모델 여유를 위해 메모리 예산 512MiB와 준비/통신 여유를 뺀 검색 시간을 전달한다. 사람 시계와 대회 전체 정책 검증은 별도다. 초기 연결은 매 요청마다 새 프로세스를 만들고 전체 BOARD를 복원하므로 캐시를 재사용하지 않는다.

빌드 후 실제 엔진 연결만 검증할 수도 있다. `1000`은 테스트용 착수 예산이며 GUI 시간 설정 변경 기능이 아니다.

```powershell
.\build\native-release\bin\omok_engine_probe.exe .\external\rapfi\Rapfi-engine\pbrain-rapfi-windows-avx2.exe 1000
```

### CMake 프리셋과 MSVC 대안
ASCII 전용 소스 경로에서는 `cmake --preset mingw-release`, `cmake --build --preset mingw-release`도 사용할 수 있다. **현재 한글 폴더에서는 위 빌드 스크립트가 검증 경로**다. 실패한 최초 `build/mingw-release` 캐시는 사용하지 않는다.

MSVC 대안은 Visual Studio 2022 Build Tools의 “Desktop development with C++”(MSVC v143·Windows SDK 포함)와 CMake를 설치한 뒤 다음과 같다. **설정은 제공하지만 이 컴퓨터에서 빌드·실행 검증하지 않았다. 설치할 필요도 없다.**

```powershell
cmake --preset msvc-release
cmake --build --preset msvc-release
cmake --install .\build\msvc-release --config Release --prefix .\dist\omok-msvc
.\dist\omok-msvc\omok.exe
```

## 프로젝트 구성
```text
src/app/      실행 진입점·manifest
src/gui/      Win32 보드·입력·상태 표시, 독립 픽셀/좌표 변환
src/state/    15×15 상태·좌표·색 배정·이력·결과 저장 모델
src/rules/    순수 렌주·구조 검사·일반 대국 200수 정책
src/opening/  준비 배치 로더·순차 복원·인계·독립 Swap2 전환
src/match/    대국 제어·기권·재시작·버전, 금수 스냅샷 작업자(시계 미구현)
src/ai/       Rapfi 프로세스·Gomocup BOARD·CPU 검사·비동기 응답
src/records/  C++ 기록 예약 경계(보존)
src/headless/ 선택 빌드 C ABI DLL
python/omok_env/ ctypes 환경·자동 대국·JSON 기록/재생·계측
scripts/      재현 가능한 빌드
tests/        CTest 상태·규칙·오프닝·match와 자체 테스트 배치
docs/         계획·기술 결정·조사·검증
licenses/     배포용 외부 라이선스 원문
```

## 상태·규칙·오프닝과 자동 테스트

기존 빌드 스크립트는 유지했고 CMake의 `BUILD_TESTING`은 기본 켬이다. 빌드 후 프로젝트 루트에서 실행한다.

```powershell
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
```

등록 구성과 마지막 실제 결과는 [처음부터 대국 검증](docs/verification-direct-start.md)을 따른다. 이전 README의86개 표기는 빈 보드 회귀 추가 전 값이었으며 당시 최종 기록은89개다. 이번 문서 작업에서 빌드/CTest를 재실행한 결과가 아니다. AI 테스트는 모의 엔진이며 실제 Rapfi/심판 대조와 구분한다. CHECK는 NDEBUG와 관계없이 동작한다. 상태·규칙·오프닝·match·좌표 계산은 Windows 독립이며 루트 빌드는 Windows x64 전용이다.

좌표는 `Coordinate{row, column}`, 왼쪽 위가 `(0,0)`이며 두 축은 0~14다. 상태 변경은 구조적 유효성을 검사하고 실패 시 전체 상태를 보존한다. 다음 돌 색은 호출자가 명시하며, 참가자 A/B·색·자신/상대·실제 배치 참가자를 구분한다. `finish`는 외부 판정 결과를 기록할 뿐 승패를 판정하지 않는다. 자세한 책임과 변환 경계는 [상태 모델 계약](docs/state-model.md)을 따른다. 이 API를 대국 제어 없이 GUI/AI 입력에 직접 연결하지 않는다.

규칙은 승리·장목·사사·재귀적 삼삼 예외·만판·준비 배치 포함 200수 경계를 판정하며 원본을 변경하지 않는다. 준비 복원은 순서대로 이 판정을 사용해 결과를 기록한다. 일반 대국의 권한·사본 적용·금수패 처리는 [match](docs/match-contract.md)가 담당한다. 상태 자체가 선이나 수순으로 자동 종료하지 않는 저장 계약은 유지한다. API는 [규칙 명세](docs/rules-spec.md)·[사례 명세](docs/renju-cases.md)를 따른다.

준비 로더의 입력은 고정 심판 client compact position(예: `h8i8g9`)이다. 문자=열 0~14, 숫자=행+1이며 내부 수순 1부터 흑/백으로 복원한다. 외부 준비 돌의 `placed_by`는 nullopt, 직접 둔 돌은 실제 A/B다. 실패 시 메타데이터·이벤트를 포함한 세션 전체를 보존한다. 공식 렌주 데이터와 사용/재배포 조건은 미확보다. 외부 데이터나 테스트 fixture는 install하지 않고 GUI 코드에 직접 작성한 개발용 배치 다섯 가지를 제공한다. 공식 대회 배치로 소개하지 않는다.

Swap2는 기본 준비 렌주와 분리된 상태 기계다. 네 선택 경로의 색·차례·주체를 관리하지만 규칙 연결은 미결로 남겨 완료 후 `RuleContractRequired`를 반환한다. 렌주·200수에 자동 연결하거나 GUI·문자열 통신을 구현하지 않았다. 전체 스냅샷·이벤트·본게임 진입 계약은 [오프닝 계약](docs/opening-contract.md)을 따른다.

선택적 실제 심판 함수 대조는 Python 3(이번 검증은 3.13.7)와 네트워크가 필요하며 추가 패키지 없이 실행한다. GUI·일반 빌드·CTest에는 Python이 필요 없다.

```powershell
python --version
python .\scripts\compare_rules.py --fetch --cache .\build\research\pinned-referee-verify --summary .\docs\verification\rules-comparison-summary.json
```

고정 GomocupJudge `56cfd7d`의 원본 판정 함수/대국 루프로 **2,264개 보드·6개 수순 경계 대조, 불일치 0**을 확인했다. 원본 소스는 해시 검증 후 build의 외부 캐시에만 둔다. 전체 매니저·엔진 연동·대회 호환성 전체 완료를 뜻하지 않는다. 상세 명령·실패 이력·제약은 [규칙 검증 기록](docs/verification-rules.md)에 구분했다.

오프닝 비교도 같은 고정 버전의 원본 함수로 실제 실행했다.

```powershell
python .\scripts\compare_opening.py --fetch --output .\docs\verification\opening-comparison.json
```

**준비 복원 3건·Swap2 4경로·원본 좌표 변환, 불일치 0**. 고정 응답 객체와 메모리 알림 콜백을 사용하고 첫 본게임 요청 직전에 멈추므로 실제 AI/통신/전체 대국 검증은 아니다. 출력·해시·원본과의 검증 계약 차이는 [오프닝 검증 기록](docs/verification-opening.md)에 있다.

## GitHub 저장과 브랜치

[GitHub 저장소](https://github.com/lee220595/omok)의 기본 브랜치는 `main`이다. 일반 작업은 `main`에서 의미 있는 단위로 검증·커밋·푸시하고, 대규모 변경·실험은 `codex/<작업명>` 브랜치를 사용한다. 기존 Git 저장소를 다시 초기화하지 않는다.

소스·공유 빌드 설정·문서는 추적하며 생성된 실행 파일·ZIP·개인 IDE 설정·비밀정보는 제외한다. .gitignore는 이미 추적된 파일을 소급해 제외하지 않으므로 커밋할 파일도 확인한다. 원격과 충돌하면 fetch 후 양쪽 작업을 보존해 통합하며 강제 푸시·원격 브랜치 삭제·작업을 버리는 초기화는 하지 않는다. 상세 운영은 AGENTS.md의 Git·GitHub 저장 정책을 따른다.

기본 창의 과거 실제 빌드·실행 검증과 미검증 범위는 아래 검증 기록에 있다. 저장 정책·제외 규칙만 변경하는 작업에서 빌드·실행을 재수행했다고 보고하지 않는다.

- [개발 지침](AGENTS.md) · [승인 기록](docs/requirements-pending.md)
- [다음 단계 개발 인계](docs/handoff.md)
- [Rapfi 설치·실행·변환/프로세스 계약](docs/rapfi-integration.md) · [실제 엔진·GUI 검증](docs/verification-rapfi.md)
- [로컬 대국·좌표·금수 작업자 계약](docs/match-contract.md) · [실제 검증·남은 수동 절차](docs/verification-match.md)
- [상태 모델 API·좌표 계약](docs/state-model.md) · [상태 구현 검증](docs/verification-state.md)
- [규칙 계약](docs/rules-spec.md) · [판정 사례](docs/renju-cases.md) · [규칙·심판 대조 검증](docs/verification-rules.md)
- [오프닝 계약](docs/opening-contract.md) · [오프닝 구현·실제 비교 검증](docs/verification-opening.md)
- [전체 개발 계획·단계별 완료 기준](docs/development-plan.md)
- [기술·라이선스 선택 근거](docs/technology-decisions.md)
- [매니저·외부 AI 후보](docs/integration-candidates.md) · [기존 공식 규칙 조사](docs/compatibility-research.md)
- [실제 검증·미검증](docs/verification-foundation.md) · [외부 배포 고지](THIRD_PARTY_NOTICES.md)
