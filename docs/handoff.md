# 오목 프로젝트 개발 인계

작성일: 2026-10-19. 코드 확인 기준: `main`의 `c2a7dd9fa5978cfdbfa60de2f45a1cc696e7112a`. 이번 변경은 인계 문서 작성이며 게임 코드는 변경하지 않는다. 아래의 **과거 실행 결과**는 [기반 검증 기록](verification-foundation.md)과 이번 대화의 실제 실행 기록을 근거로 한다. 이번 문서 작업에서 빌드·GUI 실행을 다시 수행한 것은 아니다.

## 1. 먼저 읽을 자료와 작업 경계

- [AGENTS.md](../AGENTS.md): 확정 요구사항·Git 정책·허용된 구현 범위.
- [승인 기록](requirements-pending.md): 해결된 질문과 추후 선정·기술 조사 항목. 파일명과 달리 전체가 미승인 상태는 아니다.
- [전체 개발 계획](development-plan.md), [README](../README.md): 단계별 완료 기준과 빌드 절차.
- [기술 선택](technology-decisions.md), [후보 조사](integration-candidates.md), [공식 규칙 조사](compatibility-research.md): 근거와 검증 수준.

현재 명시적으로 승인받아 구현한 범위는 프로젝트 기반·기본 GUI 창까지다. 다음 구현은 사용자의 다음 단계 지시로 범위를 확인하고 진행한다. 이 인계 문서 자체가 게임 규칙 전체·외부 AI 연동·자체 AI 개발 착수 승인을 추가하지 않는다. 승인된 제품 동작과 일반 기술 선택은 반복 질문하지 않는다.

## 2. 현재 구현 상태

| 위치 | 현재 코드에 있는 것 | 아직 없는 것 |
|---|---|---|
| `src/app/main.cpp` | `wWinMain`에서 GUI 실행 함수 호출 | 대국 구성·참가자 설정·게임 초기화 |
| `src/app/app.manifest`, `resources.rc` | x64·일반 사용자 권한·DPI 선언, manifest 리소스 | 선언만으로 대상 OS·모니터 호환성이 검증된 것은 아님 |
| `src/gui/main_window.cpp` | “오목 프로젝트” 창, 한글 안내문 GDI 표시, 메시지 루프, 창 크기 변경 재그리기·최소 크기·DPI 변경 처리 코드, 오류 대화상자·닫기 경로 | 바둑판·좌표·착수·대국 설정·금수 표시·시간·결과 화면 |
| `src/state` | `omok_state` INTERFACE 타깃 | 15×15 보드, 돌 색·참가자·차례·결과 모델 |
| `src/rules` | state에 의존하는 INTERFACE 타깃 | 합법 수·금수·승리·무승부 판정 전부 |
| `src/opening` | state에 의존하는 INTERFACE 타깃 | 준비 오프닝 복원·Swap2 상태 전환 |
| `src/match` | state/rules/opening 의존 경계 | 입력 요청·시계·기권·재시작·종료·일시정지 |
| `src/ai` | state에 의존하는 INTERFACE 타깃 | 외부 실행 파일 설정·프로세스·파이프·Gomocup·SWAP2BOARD·모의 AI |
| `src/records` | state에 의존하는 INTERFACE 타깃 | 기보 저장·불러오기·종료 후 복기 |
| 루트 CMake·빌드 스크립트 | Release 빌드, install 스테이징, CPack 설정, MinGW 경로 우회 | CTest 등록·게임 단위 테스트·CI |

실제 컴파일 대상은 `omok` 실행 파일과 `omok_gui` 정적 라이브러리다. 나머지 타깃은 분리 방향만 예약했으며 게임 기능으로 계산하지 않는다. 사람 대 사람·사람 대 AI·AI 대 AI는 모두 미구현이다. 현재 창을 완성된 게임이나 AI 엔진으로 소개하지 않는다.

## 3. 선택한 기술과 이유

| 선택 | 이유·주의점 |
|---|---|
| C++20, 확장 비활성 | 기존 GCC 13.3.0에서 빌드 가능하며 이후 상태·수명·비동기 구현의 기반. 지금 특정 AI 알고리즘을 선택한 것은 아님 |
| Win32 User32/GDI32 | Windows 전용 요구에 맞고 추가 GUI 런타임이 필요 없음. 창·레이아웃·DPI·접근성은 직접 관리해야 하며 도메인 코드와 분리 |
| CMake 3.24 이상 | 모듈별 타깃·복수 도구 체인·설치 단계 관리. 현재 설치·검증 버전은 3.29.3 |
| MinGW-w64 x64 GCC 13.3.0·Ninja 1.12.1 | 기존 설치 도구로 실제 빌드. MinGW-w64는 11.0.1. GCC 런타임을 정적 링크 |
| MSVC 2022 프리셋 | 대안 구성만 제공. 현재 빌드·실행 미검증이며 기본 도구 체인으로 바꾸지 않음 |

실행 ZIP에는 Windows 기본 DLL을 복사하지 않는다. GCC Runtime Library Exception·MinGW-w64 고지와 원문은 [외부 고지](../THIRD_PARTY_NOTICES.md)·`licenses/`에 있다. CMake·Ninja는 빌드 전용으로 동봉하지 않는다. 프로젝트 자체 공개 라이선스는 아직 선정하지 않았다. 외부 매니저·AI·모델도 현재 제품에 포함하지 않는다.

## 4. 실제 실행한 명령과 결과

### 기반 개발 시 실행한 빌드·패키지 검사

아래 명령은 프로젝트 루트 기준이다. 당시 호스트가 보고한 Windows 버전은 `10.0.26200.0`이다. 별도 Windows 10·11 클린 환경 결과가 아니다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

최종 스크립트 수정 후 연속 두 번 빌드·링크·install이 성공했고 출력된 컴파일 경고는 없었다. 이 명령은 내부에서 다음 작업을 수행한다. 내부 명령을 새로 검증한 별도 경로로 해석하지 않는다.

- 도구·x64 타깃·ASCII 임시 경로 조건 확인.
- 임시 폴더에 현재 프로젝트를 가리키는 junction 생성·검증. 소스를 이동·복사하지 않음.
- `build/native-release`의 `.ninja_deps`·`.ninja_log`만 정리.
- `cmake --fresh -S <별칭> -B <별칭>/build/native-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++`.
- `cmake --build <빌드 경로>`, `cmake --install <빌드 경로> --prefix <프로젝트>/dist/omok`.

```powershell
Compress-Archive -Path .\dist\omok\* -DestinationPath .\dist\omok-0.1.0-windows-x64.zip -Force
& 'C:\Program Files\mingw64\bin\objdump.exe' -p dist\omok\omok.exe |
    Select-String 'Magic|Subsystem|DLL Name'
Get-FileHash dist\omok\omok.exe,dist\omok-0.1.0-windows-x64.zip -Algorithm SHA256
```

- ZIP 생성 및 항목 검사 성공: 실행 파일·실행용 README·외부 고지·라이선스 원문 3개, 총 6개 파일.
- PE32+ x64, Windows GUI subsystem 확인. import 목록은 `GDI32.dll`, `KERNEL32.dll`, `msvcrt.dll`, `USER32.dll`이었다. 추가 libstdc++/libgcc/winpthread DLL 의존은 확인되지 않았다.
- SHA256을 실제 계산했으나 [당시 기록](verification-foundation.md)의 해시를 재빌드 후 파일에도 그대로 적용하지 않는다. 산출물은 Git 추적 대상이 아니며 새로 clone하면 직접 빌드해야 한다.
- CPack 설정은 있지만 실제 ZIP 검증에 사용한 것은 위 `Compress-Archive`다. `cpack` 실행 성공을 주장하지 않는다.

### 실제 GUI 확인

기반 개발 시 computer-use의 `sky.launch_app`으로 `build/native-release/bin/omok.exe`와 `dist/omok/omok.exe`를 실행했다. 첫 창의 제목·한글 안내 표시를 화면으로 확인했고, 배포 창의 닫기 단추를 실행한 뒤 창 목록에서 사라짐을 확인했다. **프로세스 종료 코드는 수집하지 않았다.** 직접 PowerShell에서 실행 파일을 호출해 같은 검증을 했다고 기록하지 않는다. 실행·종료는 수동/GUI 확인이며 자동 테스트 스위트가 아니다.

### 이번 인계 작성에서 수행한 확인

`git status --short --branch`, `git fetch origin`, `git rev-parse HEAD`, `git rev-parse origin/main`으로 작업 시작 시 로컬·원격 `main`이 위 기준 커밋으로 일치하고 작업 폴더가 깨끗함을 확인했다. 소스·CMake·스크립트·기존 문서를 읽어 구현 상태를 대조했다.

`cmake --list-presets`는 이번에도 실행 성공했으며 `mingw-release`, `msvc-release`가 표시됐다. 프리셋 인식은 해당 프리셋 빌드 성공을 뜻하지 않는다. 인계 문서 작성 후에는 로컬 링크·공백·커밋 대상 검사를 수행한다. 이번 작업에서 빌드·GUI·패키징을 다시 실행하지 않는 이유는 코드 변경이 없는 문서 작업이기 때문이다.

## 5. 실행하지 못했거나 아직 하지 않은 검증

| 항목 | 상태·이유 |
|---|---|
| 개발 도구 없는 별도 Windows 10·11 x64 실행 | 클린 PC/VM에서 검증하지 않음. 현재 호스트 실행·DLL 검사로 대체 완료 처리하지 않음 |
| MSVC 2022 빌드 | 도구 체인 설치가 확인되지 않아 실행하지 않음. 이번 `Get-Command cl`도 결과 없음; 이것만으로 모든 설치 경로의 부재를 단정하지 않음 |
| 최대화/복원·복수 모니터·실제 DPI 변경 | 초기 조작 중 도구가 `window is minimized`·`user input was detected`를 반환해 확인을 완료하지 못함. 코드상 메시지 처리와 실제 동작 검증을 구분 |
| 장시간 실행·GDI 자원 누수·오류 경로 | 별도 지속/오류 주입 검사를 하지 않음 |
| 단위·회귀·CTest | 등록된 테스트가 없고 게임 기능도 미구현. `ctest`를 실행해 통과했다고 보고하지 않음 |
| 매니저·실제 엔진 대국 및 Gomocup 통신 | 통신 모듈·프로세스 제어가 미구현이며 엔진 바이너리는 실행하지 않음. 모의 AI 검증도 아직 없음 |
| 대회 적합성·금수 예외·200수 경계·시간/자원 강제 | 대상 심판·리그 선정과 실제 규칙·연동 구현이 필요하므로 미검증 |

## 6. 알려진 문제와 미확정 사항

- **빌드 경로:** 설치된 Ninja는 한글 경로가 남은 의존성 메타데이터에서 `FindFirstFileExA` 오류를 냈고 windres는 공백 절대 include 경로를 잘못 처리했다. 현재 스크립트의 ASCII junction·메타데이터 정리와 CMake 상대 리소스 컴파일이 확인된 우회다. 현재 한글 폴더에서 프리셋 직접 빌드를 검증 경로로 바꾸지 않는다.
- **빌드 전 실행 종료:** 실행 중인 exe를 재링크하면 파일 잠금으로 실패할 수 있다. 창을 먼저 닫는다. 스크립트는 사용자 앱을 강제 종료하지 않는다. TEMP에 비ASCII 문자가 있으면 기존 ASCII 디렉터리를 `-AliasParent`로 지정한다. 기존 별칭의 대상이 다르면 스크립트는 삭제하지 않고 중단한다.
- **잠정 대회 설정:** 15×15 렌주, 준비 오프닝, 패스 금지·200수 무승부, 한 수 30초·전체 180초·엔진 메모리 1GB·한 CPU 코어·pondering 금지는 현재 문서의 잠정 기준이다. **코드에 적용된 대국 설정은 아직 없다.** 목표 연도·리그가 선정되면 공식 공고로 갱신한다. 기존 Swap2 후 렌주 조합을 기본값·추가 필수 모드로 되살리지 않는다.
- **판정·시계 명세:** 준비 오프닝의 수순 계산 포함 여부, 200번째 수와 승패 우선순위, 흑의 동시 5목·금수 및 재귀적 삼삼 예외, 초기화 시간 계측·오류 결과·자원 강제 방식을 대상 심판과 대조해야 한다.
- **매니저:** Piskvork를 외부 검증 도구로 우선 제안했고 GomocupJudge를 비교 후보로 조사했다. 최종 채택·GUI 연결 계약·실행 검증은 미완료다. Piskvork CLI 문서의 일부 단위 표현이 맞지 않아 실험 확인이 필요하며, GomocupJudge의 명시적 재배포 라이선스는 확인하지 못했다.
- **외부 AI:** Rapfi는 렌주·SWAP2BOARD 소스 확인 후보, Pela는 기본 통신·렌주 교차 검증 후보다. Pela의 SWAP2BOARD 지원은 확인 못했다. 고정 소스 버전·배포 조건은 [후보 조사](integration-candidates.md)에 있으며 바이너리 지원을 실행 검증한 것은 아니다. 외부 엔진을 자체 AI로 소개하지 않는다.
- **제품 범위:** 기보·복기·금수 표시·기권·재시작·AI 대 AI 일시정지는 승인됐지만 미구현이다. 온라인·무르기·연습용 제한 완화는 범위 밖이다. Swap2 GUI 입력 방식은 보류됐다.
- **대상 환경·라이선스:** Windows 10 최소 지원 빌드·DPI API 호환성과 클린 배포를 확인해야 한다. 자체 코드 공개 라이선스는 미선정이며 외부 도구·엔진 코드나 모델을 동봉할 때 별도 조건을 확인한다.

## 7. 다음 단계 작업과 완료 기준

1. **인계 기준 확인:** Git 상태·원격·AGENTS.md·승인 기록을 읽고 이후 사용자 변경을 우선한다. 상태·규칙 구현으로 범위가 넘어갈 때 다음 단계 지시를 확인한다. 기존 승인 항목을 다시 묻지 않는다.
2. **상태 모델:** GUI·Windows와 무관한 `state`부터 구현한다. 15×15 보드·좌표·돌·참가자 A/B·색 소유·차례·결과·착수 이력을 명시하고 판 밖·중복·색 매핑 검증을 추가한다. 완료 기준은 GUI 없이 상태 불변 조건과 좌표 변환을 검증할 수 있는 것이다.
3. **렌주 사례 명세와 규칙 모듈:** 대상 심판 근거를 고정하고 금수 정의·예외·승리 우선순위·200수 경계를 사례로 정리한다. `rules`는 상태를 받아 판정과 이유를 반환하며 GUI 판정을 복제하지 않는다. 완료 기준은 방향·가장자리·가짜 삼·재귀적 예외·동시 5목·종료 경계의 자동 회귀 검증이다. 불명확한 공식 동작을 취향으로 임의 결정하지 않는다.
4. **후속 단계:** 준비 오프닝·Swap2 전환 → 대국 제어·사람 대 사람 GUI → 모의 AI 오류 시나리오와 외부 AI 통신 → 실제 매니저·엔진 대조 → 기보·관전 → 클린 환경 통합·배포 순으로 진행한다. 자체 AI 알고리즘은 게임·외부 AI 검증 후 별도 단계다.
5. **호환성 증거 축적:** 매니저·엔진 버전·설정·원시 통신 로그를 남긴다. 판정 불일치는 중단·기록·결과 미확정, AI 오류는 조사한 대회 정책 적용·별도 재시도 제외가 승인된 정책이다. 단순 문서 지원을 실제 연동 완료로 올리지 않는다.
6. **저장·보고:** 일반 작업은 `main`, 큰 변경·실험은 `codex/<작업명>`을 사용한다. 작업 단위별 검증 후 커밋·일반 푸시하고 커밋 ID·브랜치·푸시 결과·미검증을 보고한다. `build/`, `dist/`, 개인 IDE 상태·비밀정보는 커밋하지 않는다. 강제 푸시·원격 브랜치 삭제·작업 폐기는 하지 않는다.

## 8. 저장소 상태와 찾는 방법

원격: `origin` = `https://github.com/lee220595/omok.git`. 기본 브랜치는 `main`이다. 초기 정책 `ed7fe1d`, 기반 구현 `a592c72`, 원격 이력 통합 `8a63e79`, 저장 정책 보완 `c2a7dd9`가 보존돼 있다. 새 Git 초기화나 새로운 첫 커밋을 만들 필요가 없다. 이 문서 작성 이후의 최종 커밋·푸시 결과는 해당 작업 완료 보고와 `git log`로 확인한다.
