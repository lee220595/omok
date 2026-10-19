# 오목 프로젝트 (omok)

Windows 10·11 64비트용 C++ 게임 프로젝트. 현재 버전 0.1.0의 GUI는 **기본 창만 실행**한다. GUI/Windows와 독립된 상태 모델과 CTest 자동 테스트 13개를 추가했다. 바둑판 화면·대국 진행·렌주 판정·기보·외부 AI·자체 AI는 아직 구현하지 않았다. 기본 규칙의 개발 기준은 Gomocup 렌주 일반 대회이며 목표 연도·리그는 추후 선정한다.

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

산출물:

- `build/native-release/bin/omok.exe`: 빌드 실행 파일
- `build/native-release/bin/omok_state_tests.exe`: CTest가 실행하는 상태 테스트(배포 폴더에는 넣지 않음)
- `dist/omok/`: 실행 파일·README·외부 고지·라이선스가 모인 배포 준비 폴더
- 창 제목은 “오목 프로젝트”. 기본 안내문이 표시되며 닫기(X)나 Alt+F4로 종료한다.

재빌드·실행 파일 교체 전에는 실행 중인 창을 닫는다. 그렇지 않으면 Windows 파일 잠금으로 링크가 실패할 수 있다. 빌드 스크립트가 실행 중인 앱을 임의 종료하지는 않는다. 스크립트는 이전 실패 경로가 남은 `.ninja_deps`·`.ninja_log` 두 생성 파일만 정리하고 새 CMake 구성을 수행한다.

압축 해제 후 실행 패키지를 만들려면 빌드 완료 후 다음을 실행한다.

```powershell
Compress-Archive -Path .\dist\omok\* -DestinationPath .\dist\omok-0.1.0-windows-x64.zip -Force
```

생성한 ZIP은 개발용 기본 창이다. 게임 완성본·대회 제출 엔진으로 소개하지 않는다. 사용자 실행에는 CMake·GCC·Ninja가 필요 없도록 현재 MinGW 런타임을 정적 링크했으며, 실제 DLL 검사 결과와 대상 OS 미검증 범위는 [검증 기록](docs/verification-foundation.md)을 참고한다.

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
src/gui/      기본 Win32 창
src/state/    15×15 상태·좌표·색 배정·이력·결과 저장 모델
src/rules/    규칙 판정 경계(미구현)
src/opening/  준비 오프닝·Swap2 경계(미구현)
src/match/    대국·시간 제어 경계(미구현)
src/ai/       외부 AI 프로세스·통신 경계(미구현)
src/records/  기보·복기 경계(미구현)
scripts/      재현 가능한 빌드
tests/        CTest 상태 모델 자동 테스트
docs/         계획·기술 결정·조사·검증
licenses/     배포용 외부 라이선스 원문
```

## 상태 모델과 자동 테스트

기존 빌드 스크립트는 유지했고 CMake의 `BUILD_TESTING`은 기본 켬이다. 빌드 후 프로젝트 루트에서 실행한다.

```powershell
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
```

현재 Release 빌드에서 **13/13 통과**했다. 테스트의 CHECK는 NDEBUG와 관계없이 동작한다. 상태 라이브러리는 Windows 헤더·GUI 링크를 사용하지 않으며 테스트 실행 파일도 GUI에 의존하지 않는다. 다만 루트 프로젝트는 기존 Windows x64 전용 구성을 유지하며 비Windows 빌드를 검증한 것은 아니다.

좌표는 `Coordinate{row, column}`, 왼쪽 위가 `(0,0)`이며 두 축은 0~14다. 상태 변경은 구조적 유효성을 검사하고 실패 시 전체 상태를 보존한다. 다음 돌 색은 호출자가 명시하며, 참가자 A/B·색·자신/상대·실제 배치 참가자를 구분한다. `finish`는 외부 판정 결과를 기록할 뿐 승패를 판정하지 않는다. 자세한 책임과 변환 경계는 [상태 모델 계약](docs/state-model.md)을 따른다. 이 API를 대국 제어 없이 GUI/AI 입력에 직접 연결하지 않는다.

렌주 금수·승리·200수·만판 무승부·오프닝·실제 대국·프로토콜 테스트는 아직 없다. 상태만으로 선이 완성되거나 200수를 넘었다고 게임을 종료하지 않는다. 실제 결과·미검증 범위는 [상태 검증 기록](docs/verification-state.md)에 구분했다.

## GitHub 저장과 브랜치

[GitHub 저장소](https://github.com/lee220595/omok)의 기본 브랜치는 `main`이다. 일반 작업은 `main`에서 의미 있는 단위로 검증·커밋·푸시하고, 대규모 변경·실험은 `codex/<작업명>` 브랜치를 사용한다. 기존 Git 저장소를 다시 초기화하지 않는다.

소스·공유 빌드 설정·문서는 추적하며 생성된 실행 파일·ZIP·개인 IDE 설정·비밀정보는 제외한다. .gitignore는 이미 추적된 파일을 소급해 제외하지 않으므로 커밋할 파일도 확인한다. 원격과 충돌하면 fetch 후 양쪽 작업을 보존해 통합하며 강제 푸시·원격 브랜치 삭제·작업을 버리는 초기화는 하지 않는다. 상세 운영은 AGENTS.md의 Git·GitHub 저장 정책을 따른다.

기본 창의 과거 실제 빌드·실행 검증과 미검증 범위는 아래 검증 기록에 있다. 저장 정책·제외 규칙만 변경하는 작업에서 빌드·실행을 재수행했다고 보고하지 않는다.

- [개발 지침](AGENTS.md) · [승인 기록](docs/requirements-pending.md)
- [다음 단계 개발 인계](docs/handoff.md)
- [상태 모델 API·좌표 계약](docs/state-model.md) · [상태 구현 검증](docs/verification-state.md)
- [전체 개발 계획·단계별 완료 기준](docs/development-plan.md)
- [기술·라이선스 선택 근거](docs/technology-decisions.md)
- [매니저·외부 AI 후보](docs/integration-candidates.md) · [기존 공식 규칙 조사](docs/compatibility-research.md)
- [실제 검증·미검증](docs/verification-foundation.md) · [외부 배포 고지](THIRD_PARTY_NOTICES.md)
