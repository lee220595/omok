# 로컬 사람 대 사람 구현 검증

2026-10-24. 구현 보존 기준: `5023aa239f8a4f135107efc6e0ad22f52c11a20c`(WIP, 원격 작업 브랜치에 푸시). 작업 브랜치 `codex/local-match-gui`. 이 기록의 최종 수정·테스트·고지는 후속 커밋에 함께 저장한다. 최종 커밋 ID는 완료 보고에서 확인한다. **구현·자동 검증·원격 저장과 GUI 수동 대국 검증을 구분하며, 이 단계 전체는 미완료다.**

## 시작 상태와 변경

AGENTS·최신 handoff·상태/규칙/오프닝 계약·검증 기록·실제 코드를 확인했다. 인계에 나열된 소스/헤더/CMake/테스트는 모두 존재했다. 원격 fetch 후 기존 `8c78a43`의 작업 브랜치를 이어 사용했다. 기존 main은 오프닝 구현 `a15a871`이며 이번 GUI 구현을 main에 완료 상태로 합치지 않는다. 관련 소스만 WIP로 커밋·일반 푸시했고 build/dist·개인 파일·자격증명은 포함하지 않았다.

match의 참가자 권한, 원본 Session 보존, 사본 판정/배치/종료, 구조 오류의 전체 보존, 금수패 정책, Token과 작업자의 잠금/수명을 검토했다. 규칙이나 기본 게임 모드를 변경하지 않았다. 추가 검증은 부분 금수 스캔 취소·미완료 캐시 거부와 작업자 40회 취소/교체/소멸이다. 실행 파일의 정적 winpthreads에 필요한 저작권/허가/면책 고지 누락을 보완했다. 실제 GUI 동작 결함은 도구 충돌로 확정하지 못했으며 임의 수정으로 숨기지 않았다.

## 실제 빌드와 자동 검증

환경: Windows x64 호스트, GCC 13.3.0, MinGW-w64 11.0.1(POSIX/SEH), CMake 3.29.3, Ninja 1.12.1. 기존 `scripts/build.ps1`은 변경하지 않았다.

```powershell
$matchAliasParent = Join-Path $env:TEMP 'omok-rules-build'
New-Item -ItemType Directory -Force -Path $matchAliasParent | Out-Null
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent $matchAliasParent
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
.\build\native-release\bin\omok_match_tests.exe benchmark
Get-FileHash -LiteralPath build/native-release/bin/omok.exe,dist/omok/omok.exe -Algorithm SHA256
& 'C:/Program Files/mingw64/bin/objdump.exe' -p .\dist\omok\omok.exe | Select-String 'DLL Name'
& 'C:/Program Files/mingw64/bin/nm.exe' .\dist\omok\omok.exe | Select-String 'pthread_create|pthread_join'
Get-Process -Name omok -ErrorAction SilentlyContinue
```

- 처음에는 AliasParent 폴더 부재로 실패했다. 폴더를 만든 뒤 제한된 실행에서 junction 생성 Access denied가 발생했다. 권한 검토를 거쳐 **같은 스크립트**로 다시 실행해 해결했다. 기존 폴더/연결을 삭제하거나 소스를 옮기지 않았다.
- 실행 중이던 프로젝트 창을 Alt+F4로 정상 종료한 후 첫 빌드/install 성공. 파일 잠금은 재발하지 않았다. 무관한 프로세스를 종료하거나 파일을 강제로 삭제하지 않았다.
- 테스트/고지 수정 후 최종 빌드: configure·25개 컴파일/링크 단계·install 모두 종료 코드 0. 컴파일 경고 출력 없음.
- **최종 CTest 81/81 통과**, 0 실패, 총 2.86초. 상태14·규칙39·오프닝14·match14. 이후 실행 코드 변경은 없었다. 전체 출력은 로컬 `build/match-final-ctest.txt`, 개별 출력은 `build/native-release/Testing/Temporary/LastTest.log`에 있다. 생성 로그는 Git에 포함하지 않는다.
- `match.worker_lifecycle`은 40회 생성/취소/재요청/소멸 통과, 출력상 666.254ms. 이는 스레드 생성·대기 등을 합친 회귀 검사 시간이며 단일 후보 최악 종료 지연의 상한이 아니다.
- 최종 build/dist 실행 파일 SHA-256 일치: `5A42F0893044D64304D5B3726CE7744C3AE1F06EDC57A1BFE3974F6C70F9457D`.
- 주요 지침/README/인계/계약/검증 문서의 로컬 링크 84개 존재 확인, 누락 0. git diff --check 통과. 구현 파일의 추적 여부와 build/dist 미추적도 확인했다.

### 금수 계산 시간

별도 benchmark 실행의 전체 빈 교차점 스캔(225칸 순회), 각 10회, 단위 ms:

| 배치 | 돌 수 | 최소 | 평균 | 최대 |
|---|---:|---:|---:|---:|
| basic_3 | 3 | 0.062000 | 0.076260 | 0.111200 |
| cross_threat_8 | 8 | 0.061600 | 0.078660 | 0.096200 |
| dense_synthetic | 59 | 0.046200 | 0.058340 | 0.073800 |

밀집 입력은 합성 분석 배치이며 합법 수순 도달을 보증하지 않는다. 백그라운드 스냅샷과 캐시 구조·stale 결과 거부는 자동 검증했지만, 실제 GUI 조작 중 반응성과 병적인 재귀 입력의 최악 성능은 미검증이다. 재귀 깊이 생략·시간 절단은 넣지 않았다.

## 배포 의존성과 종료

최종 import는 `GDI32.dll`, `KERNEL32.dll`, `msvcrt.dll`, `USER32.dll`뿐이다. libwinpthread-1.dll/libstdc++/libgcc DLL을 요구하지 않는다. nm로 pthread_create/pthread_join 정적 포함을 확인했다. Windows 기본 DLL은 복사하지 않는다.

install은 omok.exe, 실행용 README, THIRD_PARTY_NOTICES, licenses의 GCC 예외/GPL·MinGW·winpthreads 고지를 포함한다. [winpthreads 고지](../licenses/winpthreads-NOTICES.txt)는 설치된 pthread.h의 첫 두 저작권/허가 블록을 그대로 보존했다. [공식 v11.0.1 COPYING](https://github.com/mingw-w64/mingw-w64/blob/v11.0.1/mingw-w64-libraries/winpthreads/COPYING)의 MIT 계열·Lockless BSD 조건도 확인했다. 원본/배포 고지 해시 `767DD09158F31439986F8F29535A1880C87861EDFF616B4178FE37292CD7E1E9` 일치. 새 외부 데이터·AI·매니저 바이너리는 없다.

실행했던 GUI를 Alt+F4로 닫고 후속 Get-Process에서 omok 프로세스가 남지 않음을 확인했다. 이는 **시작 대기 상태의 정상 종료** 관찰이다. 작업자 교체/취소/join은 자동 테스트로 별도 확인했다. 실제 복잡한 금수 계산 중 닫기·장시간 핸들/GDI/메모리 누수는 미검증이다. ZIP 재생성·개발 도구 없는 별도 Windows 10/11·MSVC도 이번에 검증하지 않았다.

## 실제 GUI 확인 결과

Computer Use 스킬의 sky API로 기존 앱을 선택·정상 종료하고 첫 성공 install의 dist 실행 파일을 실행했다. 대기 화면의 제목·개발용 안내·기본 3수·흑 A/백 B 선택·0수 상태를 접근성 트리에서 확인했다. 화면에서 체크된 금수 설정과 보드를 관찰했으나 캡처의 위치/중첩이 불안정했다.

시작 버튼 시도는 `window bounds changed; call get_window_state before using this window`, 활성화/키 입력은 `user input was detected in this window; call get_window_state before continuing`을 반복 보고했다. 새 상태를 읽고 재시도했지만 대국 시작 성공은 확인하지 못했다. 시스템 메뉴/최대화를 시도한 사실을 리사이즈 검증 성공으로 기록하지 않는다. 이 도구 오류가 앱의 착수/판정 결함이라는 증거는 없다. 충돌이 지속돼 입력 검증을 중단하고 가능한 코드·자동 검증·문서·원격 저장을 진행했다. 이번 실행에서 물리 Escape 중단 메시지는 받지 않았으며 과거 Escape 중단과 구분한다.

| 항목 | GUI 관찰 결과 | 별도 자동 근거 |
|---|---|---|
| 창 시작·개발용 안내·기본 선택·시작 대기 | 통과(접근성), 시각 품질 전체는 미검증 | 시작 계약 |
| 개발 배치 변경·A/B 반대 배정·시작 | 미검증. 시작 입력 도구 실패 | start_contract/turns |
| 실제 여러 수·차례·수순·마지막 수 | 미검증 | turns/rejected |
| 승리와 종료 후 클릭 차단 | 미검증 | win |
| 금수패와 시도 돌/이력 처리 | 미검증 | forbidden |
| 200수 무승부 | 미검증 | draw |
| 이미 종료된 배치 | 미검증 | start_contract |
| 기권·재시작·준비 상태 복원 | 미검증 | resignation/restart_stale |
| 잘못된 compact 시 기존 대국 보존 | 미검증 | rejected/opening 회귀 |
| 금수 기본 켬 | 화면상 체크 관찰. 실제 표시/전환은 미검증 | hints |
| 착수·재시작 후 금수 캐시 갱신 | 미검증 | hints/worker/worker_lifecycle |
| 리사이즈·실제 클릭 위치·상단 여백/격자 | 미검증. 캡처 불안정 | geometry 전225점·DPI96/120/144/192 |
| 정상 닫기·프로세스 소멸 | 대기 상태 통과 | close/worker_lifecycle |
| 금수 계산 중 닫기·장시간 자원 | 미검증 | 자동 수명 검사만 통과 |

GUI 기능 자체의 실패 판정은 아직 없다. 도구 입력 실패와 미검증을 기능 통과로 바꾸지 않는다. 최종 재빌드 실행 파일의 전체 수동 재확인도 남았다.

## 남은 수동 확인 절차

최신 `dist/omok/omok.exe`를 실행하고, 아래에서는 특별한 언급이 없으면 흑 A/백 B로 시작한다. 좌표 레이블은 열 A..O, 행 1..15이며 위쪽이 1행이다. 한 행씩 새 대국으로 확인해 앞 대국의 수를 섞지 않는다.

| 절차 | 실제 입력 | 예상 상태 |
|---|---|---|
| 기본 진행 | 기본 3수 선택·시작 → I9 → H7 | 3수 백 B → 4수 흑 A → 5수 백 B. 마지막 H7 흑/A 붉은 원 |
| 잘못된 클릭 | 위 상태에서 H8(기존 돌), 판 바깥, 두 교차점 정중앙 | 총5수·차례·마지막 수 유지, 안내만 변경 |
| 가져오기 보존 | 총5수에서 compact를 `h8h8`로 바꾸고 시작 | 오류 원인 표시, 기존5수/색/차례 유지 |
| 기권/복원 | 총5수에서 현재 차례 기권 → 재시작 | 백 B 기권으로 흑 A 승리 → 원래3수·백 B 차례 복원. 잘못된 편집 문자열로 재시작하지 않음 |
| 승리 | 승리7수 배치·시작 → G1(백 B) → E8(흑 A) | 9수 흑 A 정확한5목 승리. 이후 F8 클릭은 변화 없음 |
| 반대 배정 | 승리7수와 흑 B/백 A 선택·시작 → G1 → E8 | 차례 백 A→흑 B, 결과 흑 B 승리. 돌 배치 순서 불변 |
| 금수/설정 | 금수8수·흑 A로 시작 → H8의 X 관찰 → 금수 표시 끄기/켜기 → H8 클릭 | 켜면 X, 끄면 사라짐, 다시 계산. H8 삼삼 반칙패·백 B 승리. 성공 이력은8수, H8 돌 없음 |
| 금수 캐시 | 금수8수 재시작 → 표시 토글 → F1(정상 흑) → 재시작 | 백 차례에서는 X 제거, 재시작 후8수·흑 차례/X 재계산. 이전 화면 결과가 남지 않음 |
| 무승부 | 무승부199수 선택·시작 → E14(백 B) | 200수 무승부. 이후 빈 점 클릭은 변화 없음 |
| 종료 배치 | 종료9수 선택·시작 | 바로 흑 A 승리/준비 배치에서 이미 종료, 착수·기권 요청 없음 |
| 크기·표시 | 기본3수로 새로 시작 → 창 크기 변경 → 빈 I9 클릭 | 변한 보드의 I9에 백이 놓임. 위 안내/열 레이블과 격자, 상태 영역이 겹치지 않음 |
| 종료/재실행 | 금수8수에서 표시 켜기와 재시작 후 즉시 X/Alt+F4 → 재실행 | 창/프로세스가 사라지고 정상 재실행 가능. 관찰 지연을 기록하고 강제 종료하지 않음 |

고정 픽셀을 재사용하지 말고 화면 레이블을 기준으로 클릭한다. 계산 재현 예시는 **클라이언트 1100×850 픽셀·DPI96**일 때 A1=(109,204), step=43, I9=(453,548), H7=(410,462), G1=(367,204), E8=(281,505), H8=(410,505), E14=(281,763)이다. 제목줄/테두리는 제외한 좌표다. 클라이언트1400×1000·DPI96에서는 A1=(182,202), step=54, I9=(614,634)로 변한다. 실제 창 크기/DPI가 다르면 이 픽셀을 그대로 사용하지 않는다.

모든 제공 배치는 프로젝트 자체 작성이며 공식 데이터/재배포 조건 확보와 무관하다. 시간·자원 제한도 없으므로 대회 설정 전체 준수를 주장하지 않는다. Swap2·AI·기보 기능을 이번 검증에 추가하지 않았다. 수동 대국 확인과 발견 결함 수정 후에야 로컬 사람 대 사람 단계 완료 및 main 통합을 판단한다.
