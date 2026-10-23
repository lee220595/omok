# 준비 오프닝·Swap2 구현 검증

2026-10-23. 구현 시작 기준은 `a7b8a56a0955d8dfc17974057f221b2bd8143dfa`다. 시작 시 작업 폴더는 깨끗했고 fetch 후 HEAD·origin/main·origin/codex/renju-rules가 같은 커밋임을 확인했다. 이전 규칙 구현의 푸시·main 반영은 완료되어 있었다. 작업 브랜치는 `codex/opening-transitions`다. 최종 커밋 ID와 푸시·통합 결과는 완료 보고와 Git 원격에서 확인한다.

## 읽은 근거와 변경 범위

AGENTS·최신 handoff·state-model·rules-spec·renju-cases·verification-rules·compatibility-research와 실제 상태·규칙·빌드 코드를 확인했다. 읽도록 지정된 문서 누락은 없었다. 과거의 오프닝 미구현/범위 제외 표시는 이번 승인으로 갱신했다.

[오프닝 계약](opening-contract.md)에 데이터/프로토콜 출처·버전·좌표·수순·허용 동작·원자성·본게임 진입·이벤트 계약을 기록했다. 구현은 표준 C++20 `omok_opening` 정적 라이브러리다. 상태의 배치 주체는 외부 준비 돌을 가상의 참가자로 기록하지 않도록 optional로 확장했다. 기존 enum·좌표·명시적 다음 색·참가자/색 분리·종료 후 거부는 유지했다. 규칙 판정 소스·GUI·기존 빌드 스크립트는 변경하지 않았다.

공식 렌주 데이터와 그 재배포 조건은 확보하지 못했다. 고정 트리의 20×20 데이터 파일을 렌주 데이터로 가져오지 않았으며 [자체 테스트 배치](../tests/fixtures/PREPARED-DATA.md)만 작성했다. 제품 install에는 외부 데이터·테스트 fixture·심판·테스트 exe를 넣지 않는다. Swap2는 구조적 프로토콜 상태 전환이며 렌주/200수 바인딩을 하지 않는다. 완료되어도 `RuleContractRequired`이면 일반 대국 진입은 불가다.

## 실제 빌드와 CTest

환경: 기존 Windows x64 호스트, GCC 13.3.0·MinGW-w64 11.0.1·CMake 3.29.3·Ninja 1.12.1. 프로젝트 루트에서 실행했다. 기존 임시 경로 우회를 유지했다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent "$env:TEMP\omok-rules-build"
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
```

구현 전 전체 빌드·install 성공, 기존 CTest **52/52 통과**를 먼저 확인했다. 구현 후 빌드·링크·install 성공, 출력 컴파일 경고 없음. 최종 CTest **67/67 통과, 실패 0**(상태 14·규칙 39·오프닝 14)이다. 이번 작업의 빌드/CTest 실패는 없었다. 아래 심판 비교 도구의 중간 실패는 별도 기록한다.

| 새 검사 그룹 | 확인한 내용 |
|---|---|
| state.prepared_actor | nullopt 이력·색 배정 후 주체 보존·잘못된 enum·종료 후 거부·초기화 |
| opening.loader | 자체 파일 파싱, 비대칭/모서리·두 자리 좌표, 대문자, malformed/중복/길이 거부, output 전체 보존, 빈 배치 |
| opening.prepared_restore | 0~3수·홀짝 다음 색/참가자·두 흑 소유자·수순과 색·외부 주체 없음 |
| opening.prepared_rejection | 배치 중 좌표/중복/색/번호/enum 실패, 기존 준비/Swap2 세션의 보드·색·차례·결과·이력·단계·메타데이터·이벤트 전체 보존 |
| opening.prepared_terminal | 9번째 흑 승리의 종료 복원, 금수 데이터 거부, 승리 후 추가 데이터 거부, 종료 후 일반 착수 불가 |
| opening.prepared_limit | 실제 교대 198/199/200수 복원, 준비 돌 포함 200수 무승부, 백의 200번째 승리 우선, 201수 거부 |
| opening.prepared_first_move | 준비 완료→기존 rules 첫 백 착수 Legal, 사본 적용 후 다음 참가자/이력, 원 오프닝 보존 |
| opening.swap2_take_black / take_white | 3돌 후 B의 두 직접 선택 경로, 색·주체·다음 색/참가자, 기존 prefix 보존 |
| opening.swap2_two_take_black / two_take_white | B 추가 2돌 후 A의 두 선택 경로, 5/6돌 인계, 규칙 미결 상태의 일반 대국 진입 차단 |
| opening.swap2_rejections | 잘못된 주체·enum·stage·동작·개수, 완료/기본 준비 세션에 대한 응답 거부 |
| opening.swap2_atomic | 초기/추가 묶음의 중간 중복·판 밖·색·번호 실패, 6번째 수 오류 때 돌과 선택 이벤트 모두 보존 |
| opening.event_history | A가 둔 백 돌과 색 소유자 구분, 실제 배치 주체, 선택·배정·완료 이벤트 순서/주체/총수, 준비 복원은 actor 없음 |
| opening.initialization | 미시작·준비 종료→Swap2 새 초기화·준비 새 초기화 계약, 이력/배정/이벤트 초기화 |

## 실제 고정 소스 대조

Python 3.13.7, 추가 패키지 없이 실행했다. [고정 GomocupJudge](https://github.com/Gomocup/GomocupJudge/tree/56cfd7dc1a0d1ec06d29f44a26931f375ad05991) 원본을 ignored `build/research/opening-referee`에 내려받고 SHA-256을 확인했다.

```powershell
python .\scripts\compare_opening.py --fetch --output .\docs\verification\opening-comparison.json
python .\scripts\compare_rules.py --cache .\build\research\pinned-referee-verify --output .\build\research\opening-stage-rules-comparison.json
```

첫 명령은 **3개 준비 배치 스냅샷 + Swap2 4경로 + 원본 좌표 변환, 불일치 0**, 두 번째는 기존 규칙 **2,264개 보드 + 6개 루프 경계, 불일치 0**이었다(각 종료 코드 0). 후자 상세 구성은 [규칙 검증](verification-rules.md)을 따른다. 새 [오프닝 비교 JSON](verification/opening-comparison.json)에 고정 소스 해시·설정·입력·C++/원본 출력·알림·불일치 목록을 저장했다.

JSON의 cpp/reference 배열 순서는 `[흑 소유자 A/B, 다음 색 B/W, 다음 참가자 A/B, 행 우선 225칸 보드(0/1/2), 수순 이력]`이다. 이력은 `행,열,색,배치 주체`를 세미콜론으로 잇고 주체 `-`는 외부 준비 돌이다. 원본 입력의 좌표 배열은 x,y(열,행)다.

오프닝 대조의 실제 범위:

- 원본 server `opening2pos`와 utility `str_to_pos/pos_to_str`를 선택 실행해 자체 작성 중심 상대 좌표 `0,0,1,0,-1,1`과 `h8i8g9`의 변환·복원을 대조했다. 전체 서버 파일 로더가 제품에 구현된 것은 아니다.
- 원본 `ai_match.__init__`로 0/3/4돌을 복원하고 C++의 보드·순서·색과 비교했다. 준비 배치의 A/B 흑 소유자는 **프로젝트에서 명시한 설정**이며 원본 생성자는 A/B 대응을 저장하지 않는다. 이 부분을 심판이 스스로 배정했다고 주장하지 않는다.
- Swap2는 원본 `play`의 오프닝 구간을 실행했다. `init_protocol`만 고정 응답 객체로 대체하고 첫 일반 대국 요청 직전에 의도적으로 중단했다. 실제 프로세스·파이프·소켓은 없다. 제어용 send/recv 알림도 메모리 콜백이다.
- 일반 대국의 다음 요청 대상은 원본이 넘겨준 engine_1 객체에서 확인했다. 수순 홀짝의 다음 색과 함께 색 소유자를 유도하고, 실제 원본의 좌표·돌 순서·응답 주체를 C++ 스냅샷과 비교했다. 기본 준비는 rule=4, 독립 Swap2 원본 실행은 rule=0·special_rule=swap2였다. 이 설정을 제품의 추가 기본 모드로 만든 것은 아니다.
- 실제 유효 입력 대조와 프로젝트의 잘못된 입력/종료 데이터 검증 계약은 구분한다. 원본 준비 생성자는 판정을 생략하므로 금수/종료 데이터의 거부 정책을 같다고 비교하지 않았다.

중간 첫 `compare_opening.py` 실행은 원본이 SWAP 알림을 보낼 때 테스트 도구에 send 콜백이 없어 KeyError로 중단했다. 규칙 C++ 실패가 아닌 비교 도구의 누락이었다. 메모리 알림 콜백을 공급하고 다시 실행해 위 결과를 얻었다. 최초 실행을 성공으로 기록하지 않는다. 원본 심판 파일과 판정 함수는 수정하지 않았다.

| 원본 파일 | SHA-256 |
|---|---|
| client/ai_match.py | `93831daa81fcbcd40f4c6e87156589c6f49c5650063e5d6d7a6198e268bbe815` |
| client/check_forbid.py | `784f5bf1c0d715078474adb89903cc62be443bc739376e1f2b371ed95f2d368b` |
| client/utility.py | `0013e5619189e54520a2d44f7f2e729fbd1826e68407d95db3af329794780785` |
| server/server.py | `b377f6df4096d8b027a4fe60e414ac90dbef2d8dbcb5694d56b9de5443e6848c` |
| doc/GomocupProtocol.md (소스 조사만) | `7bfabb633bd40f8e4f23a387f7201298ae35f3853535131fb8ab1fc9b0b1d615` |

원본 함수는 AST로 선택해 실행했고 제품으로 편입하거나 배포하지 않는다. --fetch는 누락 파일만 내려받고 다른 해시의 기존 파일을 덮어쓰지 않는다. 해시 불일치·실행 실패·대조 불일치 때 성공 종료하지 않는다. 선택적 비교 도구만 Python/네트워크가 필요하고 GUI·기본 빌드·CTest에는 필요 없다.

## 미검증·다음 단계 판단

- 공식 15×15 렌주 준비 데이터의 사용/재배포 권한과 최종 참가 연도·리그·운영 심판은 미확정이다. 자체 배치 통과를 공식 데이터 검증 완료로 표시하지 않는다.
- Swap2의 구조적 전환만 완료했다. 후속 규칙 바인딩은 미해결이며 SWAP2BOARD 문자열 송수신·미정 GUI·전략적 배치/선택은 구현하지 않았다.
- 전체 match·실제 대국·시계·자원·기보 파일·AI 프로세스/프로토콜은 미구현이다. 심판 함수 실행을 전체 매니저/엔진 통합·대회 호환성으로 확대하지 않는다.
- MSVC·다른 OS·개발 도구 없는 별도 Windows 10/11 환경이 없어 실행하지 않았다. GUI 소스와 기본 창 빌드는 유지했으나 이번에 창 표시/닫기·DPI·장시간 자원을 재검증하지 않았다. ZIP도 새로 만들지 않았다.
- 메모리 할당 실패 주입·동시 쓰기·최악 규칙 성능은 검증하지 않았다. 세션 사본 공개의 noexcept와 CTest 보존 검사는 실패 원자성을 뒷받침하지만 모든 런타임 환경을 보증하지 않는다.

**기본 준비 렌주 경로의 match·로컬 사람 대 사람 GUI 단계로 진입할 수 있다.** 준비 복원·권한 인계·종료 차단·첫 정상 착수의 규칙 연계가 검증되었다. match는 게임 상태 사본·세션 수명·실제 입력 권한·판정 적용을 소유해야 한다. 공식 데이터 미확보·Swap2 규칙 미결은 별도로 남기고, 임의의 공식 오프닝/규칙 조합으로 채우지 않는다.
