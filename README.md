# 오목 프로젝트

Windows 10·11 64비트용 C++ 게임 프로젝트. 현재 버전 0.1.0은 **기본 GUI 창만 실행**한다. 바둑판·대국·렌주 판정·기보·외부 AI·자체 AI는 아직 구현하지 않았다. 기본 규칙의 개발 기준은 Gomocup 렌주 일반 대회이며 목표 연도·리그는 추후 선정한다.

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
src/state/    게임 상태 경계(미구현)
src/rules/    규칙 판정 경계(미구현)
src/opening/  준비 오프닝·Swap2 경계(미구현)
src/match/    대국·시간 제어 경계(미구현)
src/ai/       외부 AI 프로세스·통신 경계(미구현)
src/records/  기보·복기 경계(미구현)
scripts/      재현 가능한 빌드
docs/         계획·기술 결정·조사·검증
licenses/     배포용 외부 라이선스 원문
```

현재 게임 로직 테스트나 CTest 항목은 없다. 빈 모듈을 테스트 통과로 계산하지 않는다. 다음 단계에서 규칙·프로토콜의 실제 동작에 맞춘 검증을 추가한다.

- [개발 지침](AGENTS.md) · [승인 기록](docs/requirements-pending.md)
- [전체 개발 계획·단계별 완료 기준](docs/development-plan.md)
- [기술·라이선스 선택 근거](docs/technology-decisions.md)
- [매니저·외부 AI 후보](docs/integration-candidates.md) · [기존 공식 규칙 조사](docs/compatibility-research.md)
- [실제 검증·미검증](docs/verification-foundation.md) · [외부 배포 고지](THIRD_PARTY_NOTICES.md)
