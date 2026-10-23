# 오목 프로젝트 개발 인계

갱신일: 2026-10-24. **로컬 GUI 대국 단계는 수동 검증이 남아 미완료다. 구현·자동 검증·배포 스테이징·원격 저장은 수행했다.**

## 기준과 Git 저장

- 오프닝 단계 기준: `a15a871491b74c1685d1dad072c9edb392abc6f7`(기존 main·원격 반영 확인).
- 이번 구현 보존 기준: **`5023aa239f8a4f135107efc6e0ad22f52c11a20c`**, WIP 커밋. 관련 match·GUI·좌표·헤더·CMake·테스트 소스를 모두 추적하고 `origin/codex/local-match-gui`에 일반 푸시했다. 이전 문서만 저장된 상태를 해소했다.
- 후속 최종 수정은 작업자 수명/부분 취소 회귀, winpthreads 배포 고지, 최신 계약·실행 안내·검증 기록이다. 최종 커밋 ID와 푸시 결과는 완료 보고에 남긴다. 실제 상태는 `git status`, `git log`, 원격을 확인한다.
- 작업 브랜치: `codex/local-match-gui`. **main에는 이번 구현을 합치지 않는다.** 수동 대국 검증이 남아 단계 완료 기준을 충족하지 않았기 때문이다. 강제 푸시·브랜치 삭제·변경 폐기는 하지 않았다.

## 최신 승인과 자료

사용자는 match·로컬 사람 대 사람 GUI 구현에 이어 WIP 보존·빌드/install 해결·수동 GUI 검증 재개·자동 테스트·문서/저장을 승인했다. AGENTS.md의 과거 범위 제한을 이 승인에 맞춰 갱신했다. 이번에는 AI·Swap2 GUI/규칙 연결·시계/자원 강제·기보 파일/복기·자체 AI로 확대하지 않았다.

먼저 [AGENTS](../AGENTS.md), [README](../README.md), [match·좌표·작업자 계약](match-contract.md), [최신 실제 검증과 남은 수동 절차](verification-match.md)를 읽는다. [상태](state-model.md)·[규칙](rules-spec.md)·[판정 사례](renju-cases.md)·[오프닝](opening-contract.md)·[호환성 조사](compatibility-research.md)도 유지한다. 과거 [오프닝 검증](verification-opening.md)과 [규칙 검증](verification-rules.md)은 당시 심판 함수 대조 기록이며 이번에 다시 실행하지 않았다.

## 구현된 내용과 경계

- 독립 C++20 state/rules/opening을 match가 연결한다. 원본 Session 전체와 별도 진행 상태를 보존하며 준비 RenjuReady에서만 일반 착수한다. Terminal은 결과 표시만, 미완료/미결 Swap2는 차단한다.
- 참가자 A/B·색·실제 배치 주체를 구분한다. 권한과 Token 검사, 판정·배치·교대·종료·이벤트를 사본에서 적용한다. 구조 오류는 전체 상태 보존, 흑 금수는 백 승리 종료이며 시도 돌을 성공 이력에 추가하지 않는다.
- 기권·동일 준비 상태로 재시작·정상 닫기, 세션/revision으로 이전 입력 거부, 메모리 종료 사유/이벤트를 구현했다. 기보 파일과 과거 세션 영구 보관은 없다.
- Win32 15×15 보드·흑백 돌·마지막 수·상태/결과·개발 배치/색 선택·compact 입력·클릭 착수·기권/재시작 UI를 작성했다. 실제 사용자 대국 동작은 아래 미검증 상태다.
- 금수 표시는 기본 켬/흑 차례에만 분석한다. 단일 표준 스레드·불변 사본·최신 요청 합치기·취소·캐시를 사용하며 Token이 다른 결과는 폐기한다. 실제 착수는 최신 match에서 별도 판정한다. 재귀 깊이를 자르지 않는다.
- `src/gui/board_geometry.*`에 클라이언트 픽셀↔행/열 변환을 모았다. 그리기와 클릭이 동일한 크기/DPI 기반 기하를 사용한다.
- 소스는 `src/match/`와 공개 `include/omok/match/`, `src/gui/`, `tests/match_tests.cpp`에 있다. ai/records만 INTERFACE 경계로 남았다. 빌드 스크립트와 ASCII 경로 우회는 변경하지 않았다.

## 시작 배치

`src/match/development_positions.cpp`에 직접 작성한 개발용 다섯 배치다. 공식 준비 데이터·사용/재배포 조건은 미확보며 공식 배치로 소개하지 않는다. GUI에도 개발용·시계/자원 제한 미구현을 표시한다.

| ID | 수/내용 | 확인용 다음 입력 |
|---|---|---|
| dev-basic-v1 | 3수 `h8i8g9` | 백 I9, 흑 H7로 교대 확인 |
| dev-win-v1 | 7수 `a8a1b8c1c8e1d8` | 백 G1 → 흑 E8 승리 |
| dev-forbidden-v1 | 8수 `g8a1h7c1i8e1h9g1` | 흑 H8 삼삼 반칙패 |
| dev-draw-199-v1 | 자체 줄무늬 패턴에서 교대 생성 | 백 E14로 200수 무승부 |
| dev-finished-v1 | 9수 `a8a1b8c1c8e1d8g1e8` | 이미 흑 승리, 입력 금지 |

## 실제 명령과 결과

Windows 호스트, GCC13.3.0·MinGW-w64 11.0.1 POSIX/SEH·CMake3.29.3·Ninja1.12.1.

```powershell
$matchAliasParent = Join-Path $env:TEMP 'omok-rules-build'
New-Item -ItemType Directory -Force -Path $matchAliasParent | Out-Null
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent $matchAliasParent
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
.\build\native-release\bin\omok_match_tests.exe benchmark
Get-FileHash -LiteralPath build/native-release/bin/omok.exe,dist/omok/omok.exe -Algorithm SHA256
& 'C:/Program Files/mingw64/bin/objdump.exe' -p .\dist\omok\omok.exe | Select-String 'DLL Name'
```

- 기존 앱을 정상 Alt+F4로 닫아 파일 잠금을 해소했다. 처음 AliasParent 부재·제한 환경 junction Access denied를 기록했고, 디렉터리 생성과 권한 검토 후 같은 스크립트로 성공했다. 강제 종료/삭제를 사용하지 않았다.
- **최종 configure·컴파일·링크·install 모두 성공(0), 컴파일 경고 출력 없음.** build/dist SHA-256은 `5A42F0893044D64304D5B3726CE7744C3AE1F06EDC57A1BFE3974F6C70F9457D`로 일치한다.
- **최종 CTest 81/81 통과**, 2.86초. 상태14·규칙39·오프닝14·match14. 부분 취소와 40회 작업자 수명 검사 포함. 이후 실행 코드 변경 없음. 생성 로그는 `build/match-final-ctest.txt` 및 CTest LastTest.log이며 Git 제외다.
- 금수 225칸 순회 각10회 평균: 기본3수 0.076260ms, 위협8수 0.078660ms, 합성밀집59수 0.058340ms. 최악 재귀 시간·GUI 반응성을 보증하지 않는다. 작업자40회 검사 666.254ms는 전체 회귀 수행 시간이다.
- DLL은 GDI32/KERNEL32/msvcrt/USER32만 요구한다. nm로 정적 pthread_create/join을 확인하고 `licenses/winpthreads-NOTICES.txt`를 추가해 install했다. 최종 고지/원문 해시도 일치했다. 별도 libwinpthread DLL은 필요 없다. 배포 고지와 licenses 폴더를 함께 유지한다.

## GUI 관찰·남은 문제

첫 성공 install의 앱 실행·개발용 안내·선택 컨트롤·시작 대기0수와 기본 켬 체크를 관찰했다. 창 위치/캡처가 불안정했으며 시작/입력은 도구의 `window bounds changed`와 `user input was detected in this window` 오류가 반복돼 성공을 확인하지 못했다. 재관찰/재시도 뒤 입력 검증을 중단했다. 이번에는 과거의 물리 Escape 중단과 달리 해당 중단 메시지를 받지 않았다.

**미검증:** 실제 시작·여러 수·승리/금수패/무승부·종료 배치·기권/재시작·잘못된 compact 보존·표시 토글/캐시 갱신·리사이즈 클릭·수정된 여백/격자. 해당 자동 테스트 통과를 GUI 통과로 대신하지 않는다. 최종 재빌드 파일의 전체 수동 확인도 남았다. 항목별 상태·정확한 배치/착수 좌표·예상 결과·DPI96 픽셀 재현 예시는 [수동 확인 절차](verification-match.md)에 있다.

대기 상태 정상 닫기 후 Get-Process에서 omok가 남지 않음을 확인했다. 작업자 취소/교체/join은 자동 검증했지만, 복잡한 금수 계산 중 실제 창 닫기·최악 후보의 종료 지연·장시간 자원 누수는 미검증이다. 후보 내부 재귀는 정확성을 위해 생략하지 않으므로 한 후보 완료까지 종료가 지연될 수 있다. 캡처/입력 도구 오류를 앱 결함으로 확정하지 않았다. 새로 재현한 게임 로직 결함은 없으나 미검증 GUI의 결함 부재를 보증하지 않는다.

## 다음 작업

1. 작업 브랜치/원격과 추적된 구현을 확인한다. 이전의 “문서만 원격에 있음” 상태는 WIP 저장으로 해소됐다.
2. 사용자 입력과 충돌하지 않는 GUI 환경에서 검증 기록의 표를 따라 실제 클릭·각 종료 경로·리사이즈·닫기를 확인한다. 입력 중단 신호를 받으면 즉시 멈추며 자동 재개하지 않는다.
3. 발견한 문제만 수정하고 필요한 회귀/빌드를 수행한다. 결과를 verification-match와 이 인계에 갱신한 뒤 검증된 작업을 커밋·푸시한다.
4. 수동 대국 완료 기준을 충족한 뒤 기존 이력을 보존하는 main 통합을 진행한다. 현재는 **로컬 사람 대 사람 단계 전체 미완료**이며 main 미반영이다.
5. 외부 AI용 match/Token 경계는 마련됐지만 단계 전환 완료 판정은 보류한다. 수동 GUI 확인과 별도로 실제 엔진/매니저·시간/자원·오류 정책·프로토콜 로그 검증이 이후 필요하다. 이번 작업에 AI 기능을 추가하지 않았다.

MSVC, 개발 도구 없는 별도 Windows10/11, ZIP 재생성, 전체 엔진/매니저, 공식 데이터, 최종 연도/리그, Swap2 규칙 바인딩은 여전히 미검증/미확정이다. 현재 결과를 Gomocup 대회 전체 준수로 표시하지 않는다.
