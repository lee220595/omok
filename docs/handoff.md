# 오목 프로젝트 개발 인계

갱신일 2026-10-23. **이번 오프닝 구현의 기준 커밋은 `a7b8a56a0955d8dfc17974057f221b2bd8143dfa`**다. 시작 시 작업 폴더는 깨끗했고 fetch 후 HEAD·origin/main·origin/codex/renju-rules가 일치해 이전 규칙 구현의 푸시·main 반영을 확인했다. 이번 작업 브랜치는 `codex/opening-transitions`다. 최종 커밋 ID·푸시·main 통합 결과는 완료 보고와 Git 원격을 확인한다.

## 최신 승인과 읽을 자료

사용자는 준비 배치 복원·본게임 인계·별도 Swap2 상태 전환·조사·자동 테스트를 승인했다. 과거 인계의 오프닝 구현 제외보다 이번 지시가 우선한다. 바둑판 GUI·전체 match·시계/자원·외부 AI 프로세스/통신·자체 AI는 이번 범위 밖이다. 승인된 기술 선택은 반복 승인받지 않는다.

- [AGENTS](../AGENTS.md), [상태 계약](state-model.md), [규칙 계약](rules-spec.md), [오프닝 계약](opening-contract.md).
- [최신 검증](verification-opening.md), [실제 오프닝 비교 출력](verification/opening-comparison.json), [판정 사례](renju-cases.md).
- [README](../README.md), [전체 계획](development-plan.md), [승인 기록](requirements-pending.md), [호환성 조사](compatibility-research.md), [매니저/엔진 후보](integration-candidates.md).
- 과거 [상태 검증](verification-state.md)·[규칙 검증](verification-rules.md)·[기반 검증](verification-foundation.md)은 당시 결과를 보존한 기록이다. 현재 코드·최신 사용자 지시·오프닝 검증을 우선한다.

지정 문서와 참조 문서는 모두 존재했다. 과거의 “오프닝 미구현”과 상태 13/전체 52개 표시는 당시 범위이며 최신 문서에서 갱신했다.

## 현재 구현과 미구현

| 부분 | 현재 기능 | 미구현·제약 |
|---|---|---|
| app/gui | 기존 Win32 기본 창·한글 안내·메시지 루프 | 바둑판·입력·실제 대국·금수 표시. GUI 소스는 이번 무변경 |
| state | 독립 C++20 상태 저장·15×15·A/B·색·명시적 다음 색·결과·225개 이력·구조 거부 | 권한·판정·동기화·직렬화. placed_by는 이번 optional로 확장 |
| rules | 독립 렌주·재귀 삼삼·사사·장목·승리·만판·일반 대국 200수 판정 | 결과 자동 적용·일반 대국 제어·전체 규칙 호환성 보증. 이번 소스 무변경 |
| opening | compact 로더·순차 준비 복원·종료/200수 처리·권한 인계·별도 Swap2 모든 전환·이벤트 | 공식 렌주 데이터/재배포 조건 미확보, Swap2 규칙 바인딩 미해결 |
| match/ai/records | CMake INTERFACE 경계 | 전체 대국·시계/자원·AI 프로세스/통신·기보 파일/복기 전부 |
| tests | 상태 14·규칙 39·오프닝 14 = CTest 67개, 자체 배치와 선택적 실제 심판 함수 대조 | 실제 엔진/매니저/GUI 대국 검증 |

공개 오프닝 헤더는 `src/opening/include/omok/opening/opening.hpp`, 구현은 `src/opening/opening.cpp`다. `tests/opening_tests.cpp`와 자체 `tests/fixtures/prepared-self-authored.txt`를 사용한다. GUI에 상태·규칙·오프닝을 연결하지 않았으며 기본 실행 파일은 여전히 기본 창이다.

## 선택과 핵심 계약

C++20·Win32 User32/GDI32·CMake 3.24+·MinGW x64/Ninja와 기존 ASCII junction 빌드를 유지했다. opening은 표준 라이브러리와 state/rules만 사용한다. 새 테스트 프레임워크·런타임·외부 데이터 의존성을 추가하지 않았다.

`load_compact`는 고정 GomocupJudge client의 15×15 compact 좌표열을 읽는다. 문자는 열 x, 숫자는 행 y+1이고 원본 순서로 흑부터 교대한다. 예: `h8i8g9`는 (7,7) 흑·(7,8) 백·(8,6) 흑이다. 서버의 중심 상대 CSV·PSQ·BOARD·SWAP2BOARD 파서는 아니다. 파일 한 줄을 읽어 문자열로 전달할 수 있으나 파일 선택/목록 UI는 없다. 파싱 실패는 output 전체를 보존한다.

`restore_prepared(data, black_owner)`는 새 준비 세션 초기화다. 성공하면 이전 세션 전체를 교체하고 실패하면 보드·배정·차례·결과·이력·단계·id·이벤트를 모두 보존한다. 번호·색·길이·좌표·중복을 확인하고 각 수에 rules::analyze_board를 사용한다. 일반 대국용 evaluate를 매 수에 적용하지 않는다. 금수는 준비 데이터 오류로 거부하고 반칙패로 만들지 않는다. 최종 승리는 종료로 복원하며 뒤에 추가 수가 있으면 전체 거부한다. 준비 돌 포함 총 200수에서 승리 우선, 아니면 Draw다. 201수 이상은 거부한다.

`Placement::placed_by`는 optional이며 nullopt는 외부 준비 돌을 뜻한다. 실제 일반 착수/Swap2는 A/B를 기록한다. 색 소유자나 실제 배치 주체가 불명확하다는 의미와 혼동하지 않는다. 기존 저장 API는 구조만 검사하고 일반 입력 권한은 이후 match가 확인한다. 기존 A/B aggregate 호출은 그대로 가능하며 잘못된 enum은 계속 거부한다.

원본 Judge의 prepared __init__은 배열에 직접 복원하며 금수/종료를 검사하지 않는다. 제품은 사용자 검증 요구와 공식 판정을 따르는 엄격한 가져오기 계약을 추가했다. 원본의 잘못된 입력 처리까지 동일하다고 보고하지 않는다.

Swap2는 Mode::Swap2Protocol로 기본 PreparedRenju와 분리했다. A가 1흑/2백/3흑을 놓고 B가 흑 선택(0돌), 백 선택+4번째 백(1돌), 추가 4백/5흑(2돌) 중 선택한다. 추가 2돌이면 A가 흑 선택(0돌) 또는 백 선택+6번째 백(1돌)을 한다. 모든 묶음은 원자적이며 주체·단계·개수·색·번호·좌표가 틀리면 전체 보존한다. 색 선택으로 기존 돌·배치 순서는 바뀌지 않는다. 백 선택과 착수를 결합한 것은 프로토콜 의미이며 미정 GUI 흐름을 확정한 것이 아니다.

Swap2 완료는 색·다음 차례만 확정한다. 규칙·종료 정책을 렌주에 자동 결합하지 않으며 handoff는 RuleContractRequired, can_enter_main_game은 false다. 후속 바인딩 근거가 필요하다. 문자열 송수신·전략은 없다.

후속 match는 Session 전체 복사본의 game/handoff/events/data_id를 함께 받는다. RenjuReady에만 일반 착수를 요청하고 Terminal/NotStarted/RuleContractRequired에는 요청하지 않는다. OpeningInput은 별도의 input_actor를 사용한다. game의 다음 색 소유자와 오프닝 입력 주체는 다를 수 있다. match는 자체 가변 상태·실제 권한·판정/배치/종료 적용·세션 수명을 소유해야 한다.

Started/BatchPlaced/ChoiceMade/ColorsAssigned/Completed 이벤트는 주체·선택·흑 소유자·총 배치 수를 기록한다. mode/id·좌표 이력·이벤트·결과를 함께 보존해야 향후 기보 복원이 가능하다. 이번에는 메모리 내 기록만 있고 기보 파일 저장은 없다.

## 데이터 출처와 비교 기준

공식 안내와 고정 `Gomocup/GomocupJudge@56cfd7dc1a0d1ec06d29f44a26931f375ad05991`의 server/utility/ai_match·프로토콜을 조사했다. 공식 15×15 렌주 데이터와 사용/재배포 허가는 확보하지 못했다. 트리의 20×20 배치를 렌주로 가져오지 않았고 외부 데이터는 제품에 동봉하지 않는다. 자체 테스트 배치는 버전 1로 직접 작성했고 공식 대회 배치·전략으로 소개하지 않는다. 프로젝트 전체 제3자 공개 라이선스는 미선정이다.

실제 오프닝 비교는 고정 원본 함수·Swap2 구간을 실행하되 고정 응답 객체와 메모리 알림 콜백을 사용했다. 첫 본게임 요청 직전에 멈추므로 실제 AI/프로토콜/전체 매니저 검증이 아니다. 기본 준비 비교는 rule=4, 독립 Swap2 원본 비교는 rule=0·special_rule=swap2였으며 이를 제품 모드 추가로 해석하지 않는다. 최종 참가 연도·리그는 미정이다.

## 실제 명령·결과

기존 Windows 호스트, GCC 13.3.0·MinGW-w64 11.0.1·CMake 3.29.3·Ninja 1.12.1·Python 3.13.7에서 실행했다. 시작 빌드와 기존 52개 테스트가 먼저 통과했다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent "$env:TEMP\omok-rules-build"
ctest --test-dir .\build\native-release --output-on-failure --no-tests=error
python .\scripts\compare_opening.py --fetch --output .\docs\verification\opening-comparison.json
python .\scripts\compare_rules.py --cache .\build\research\pinned-referee-verify --output .\build\research\opening-stage-rules-comparison.json
```

최종 빌드·링크·install 성공, 출력 컴파일 경고 없음. CTest **67/67 통과**. 준비 복원 3건·Swap2 4경로·원본 좌표 변환 비교에서 **불일치 0**, 기존 규칙 2,264개 보드·6개 루프 경계도 **불일치 0**이었다. 실제 로그·해시는 검증 기록/JSON을 따른다. Python은 선택적 비교에만 필요하며 일반 빌드/CTest는 필요 없다.

첫 오프닝 비교 도구 실행은 원본 SWAP 알림에 필요한 send 콜백 누락으로 KeyError가 발생했다. 메모리 콜백을 추가해 재실행했고 원본 소스는 수정하지 않았다. 이 실패를 성공으로 보고하지 않는다. 빌드/CTest 자체의 실패는 없었다.

## 남은 조건과 다음 작업

- 기본 준비 렌주를 기반으로 **대국 제어·로컬 사람 대 사람 GUI 개발에 진입 가능**하다. 복원·종료 차단·인계·첫 정상 착수의 rules 연계·전체 보존·67개 회귀가 근거다. 전체 대국·대회 호환성 완료는 아니다.
- match가 실제 참가자 권한, 일반 착수의 사본 판정/배치/종료 공개, 세션 버전, 기권·재시작·종료 상태를 설계·구현해야 한다. 새 작업의 승인 범위에 맞춰 진행한다.
- GUI는 match를 통해 입력하고 rules를 중복 판정하지 않는다. 입력 주체·돌 색·소유자·오프닝 continuation을 구분한다. 실제 GUI 화면·금수 표시 반응성과 오프닝 선택 UI는 후속이다.
- 공식 준비 데이터·사용/재배포 조건·최종 참가 연도/리그/운영 심판은 미확정이다. 자기 테스트 데이터를 공식 자료로 채우지 않는다. Swap2 규칙 연결도 별도 근거가 필요하며 미정 GUI는 실제 설계 시 확인한다.
- 전체 매니저/엔진·AI 통신/프로세스·시계/자원·기보 파일은 미구현이다. 모의 응답 검증과 실제 엔진 검증을 구분한다.
- MSVC·개발 도구 없는 별도 Windows 10/11·비Windows·메모리 할당 실패 주입·동시 쓰기·최악 성능은 미검증이다. 이번 GUI 표시/닫기·DPI·장시간 자원·ZIP 재생성도 수행하지 않았다.
- 기존 빌드 스크립트·ASCII 경로 우회를 유지한다. 기본 TEMP 별칭이 충돌하면 임의 삭제하지 말고 검증된 별도 AliasParent를 쓴다. 실행 중 GUI의 파일 잠금은 창을 닫고 빌드한다.
