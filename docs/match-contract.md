# 로컬 대국·좌표·금수 작업자 계약

2026-10-24. 보존한 구현 기준은 `5023aa2`(WIP)다. 공개 API는 `src/match/include/omok/match/`, 구현은 `src/match/`다. [상태](state-model.md)·[규칙](rules-spec.md)·[오프닝](opening-contract.md) 계약을 사용하며 판정을 복제하지 않는다. 실제 검증과 미검증은 [검증 기록](verification-match.md)을 따른다.

## 대국 제어와 원자성

Controller는 GUI/Windows 독립 C++20 값 객체다. 한 작성자가 관리하며 읽기 전용 game(), source_opening(), events()를 공개한다. span/참조는 수명과 상태 변경을 가로질러 보관하지 않는다. 비동기 작업에는 사본을 전달한다.

| API | 조건과 책임 |
|---|---|
| start(Session) | 비어 있지 않은 PreparedRenju의 RenjuReady/Terminal 또는 DirectRenju의 RenjuReady를 수용. 원본 Session 전체(보드·이력·배정·차례·결과·오프닝 이벤트·데이터 ID)를 보존하고 진행용 GameState를 별도로 복사. 새 세션 ID·revision 0 |
| start_direct(black_owner) | 흑 소유자를 명시한 빈 보드 로컬 렌주 대국. 처음부터/준비 배치 선택을 구분 |
| start_compact(id,text,black_owner) | 기존 로더/복원 후 start. 실패하면 기존 Controller 전체 보존 |
| place(Token,actor,position) | 정확한 세션/revision·Playing·유효한 A/B·다음 참가자·색 소유 확인 후 rules::evaluate. 실제 actor를 반드시 공급 |
| resign(Token,actor) | 진행 중인 유효 A/B의 기권. 자기 차례일 필요는 없음. GUI 버튼은 현재 차례 참가자를 공급 |
| restart(Token) | 원래 시작 Session으로 복원하고 새 세션 ID 발급. 변경 중인 compact 입력/선택은 적용하지 않음 |
| close(Token) | 진행 중이면 Aborted/Closed로 종료. 이미 끝났으면 결과·종료 사유를 보존하고 Phase만 Closed로 변경 |

NotStarted, OpeningInput, RuleContractRequired는 start/일반 착수를 허용하지 않는다. Terminal은 결과 표시만 가능하다. Swap2 완료를 PreparedRenju로 취급하지 않는다. 2026-10-24 후속 요청으로 빈 보드는 별도 `DirectRenju` 시작 방식으로 허용한다. PreparedRenju의 빈 배치는 여전히 EmptyOpening으로 거부한다.

`start_direct(black_owner)`는 opening의 begin_direct_renju로 만든 빈 Session 전체를 기존 start에 전달한다. 0수·흑 선공·시작 이벤트·세션 토큰을 새로 설정하고 기존 대국을 원자적으로 대체한다. 일반 착수/금수패/승리/200수/기권/엔진 오류 계약은 동일하며 첫 실제 돌부터 placed_by에 참가자를 기록한다. restart는 원래 DirectRenju Session으로 복원해0수·흑 차례로 돌아간다. 초기화 실패/오래된 입력은 전체 상태를 보존한다.

모든 쓰기는 Controller 사본에서 수행한다. 구조 오류·잘못된 참가자·Token·준비 입력은 보드뿐 아니라 원본 오프닝·이력·차례·결과·이벤트·Token 전체를 보존한다. 후보 사본의 배치/결과 저장과 이벤트 기록이 모두 성공한 후 noexcept move assignment로 공개한다. 할당 예외는 전파되며 기존 객체를 보존한다. 할당 실패 주입은 미검증이다.

판정 결과 적용:

- Invalid: 오류를 반환하고 상태 보존. 잘못된 GUI 좌표를 반칙패로 만들지 않는다.
- Legal: 실제 배치 기록과 명시적 색 교대, Placement 이벤트, revision 증가.
- Win/Draw: 배치를 기록한 뒤 finish. 다음 색/참가자는 없다. 200수에는 준비 돌을 포함하며 기존 rules의 승리/금수 우선순위를 유지한다.
- Forbidden: 상대 승리·Forbidden 종료. 금수 시도 돌을 보드/성공한 배치 이력에 추가하지 않고 Ended 이벤트에 actor·후보 좌표·판정 이유를 기록한다. 재시도 정책이 아니다. [RIF 9.2](https://www.renju.net/rifrules/)와 고정 Judge의 반칙패 처리에 따른다. 준비 데이터의 금수는 기존 opening 계약대로 가져오기 오류다.

이벤트는 Started/Restarted/Placement/Ended이며 해당 Token·actor·좌표·판정·종료 사유를 보존한다. 현재 세션만 보관하고 재시작 시 초기화한다. source_opening의 원본 이벤트/ID는 진행 중 바뀌지 않는다. 이벤트만으로 기보 전체를 저장할 수 없으며 파일/복기는 후속이다. session/revision 소진은 VersionExhausted로 거부하며 재사용하지 않는다.

## 화면 좌표 경계

`src/gui/board_geometry.hpp/.cpp`가 그리기와 클릭의 유일한 변환 경계다. 상태는 row/column 0..14, 화면 표시는 열 A..O와 행 1..15, 원점은 왼쪽 위다. 픽셀은 Win32 클라이언트 영역 기준이며 제목 표시줄·창 테두리는 포함하지 않는다. DPI는 96 기준 논리 치수를 실제 픽셀로 바꾼다.

BoardGeometry의 left/top은 첫 교차점, step은 교차점 간격이다. point({r,c}) = {left+c*step, top+r*step}. hit는 바깥 격자 경계를 거부하고 가장 가까운 교차점에서 0.35*step 이내의 원형 영역만 수용한다. 그 외 클릭은 무시한다. 중복/종료 후 입력은 match가 다시 검사한다. 유효하지 않은 크기/DPI에서는 클릭·그리기를 하지 않는다.

board_geometry는 보드 위쪽 240 논리 픽셀과 우측 상태 영역을 확보한다(2026-10-24 Rapfi 방식/경로 행 추가). WM_SIZE·WM_DPICHANGED에서 레이아웃과 그리기를 갱신하고 클릭도 같은 새 geometry를 계산한다. GUI는 state를 직접 수정하거나 rules를 독자 판정하지 않는다. DPI96/120/144/192의 전 교차점과 리사이즈는 자동 계산 테스트로 검증하며 실제 모니터 DPI 이동 검증과 구분한다. 이전 검증 문서의 고정 픽셀 예시는 역사 기록이며 새 화면에 재사용하지 않는다.

## 외부 엔진 결과 적용 확장

2026-10-24 사용자 승인으로 engine_failure(Token,actor,failure)를 추가했다. 현재 Token·Playing·유효 actor·다음 참가자를 확인하고 사본에서 Ended 이벤트/결과를 공개한다. Start는 승패 없는 Aborted/EngineStartFailure이며 Timeout·Exited·BadOutput·IllegalMove는 상대 승리로 끝낸다. 일반 state 오류를 자동 변환하지 않고, AI 어댑터가 실제 엔진 입력임을 확인한 뒤 호출한다. 외부 엔진의 정상 좌표는 기존 place를 거쳐야 하며 흑 금수는 기존 Forbidden 정책이다. 늦은 응답/취소는 적용하지 않는다. 상세 프로세스/로그/시간 계약은 [Rapfi 연결](rapfi-integration.md)을 따른다.

## 금수 표시와 수명

GUI의 show_hints 초기값은 true다. Playing의 흑 차례만 분석을 요청한다. 시작·착수·재시작·설정 변경·종료 시 캐시를 지우고 기존 요청을 취소한다. 실제 착수는 표시 계산과 무관하게 match가 최신 상태로 판정한다.

- HintInput은 Token과 GameState 사본을 소유한다. compute_forbidden은 기존 analyze_board를 사용해 빈 교차점 전체를 흑 기준으로 검사한다. 입력 상태를 변경하지 않는다.
- HintWorker는 한 std::thread와 잠금·조건변수·세대 번호를 사용한다. 대기 요청은 최신 하나로 합치며 submit/cancel 시 세대를 바꾼다. 분석은 잠금 밖에서 실행하고 취소/교체된 결과는 게시하지 않는다.
- 취소는 각 후보 사이에서 확인한다. 후보 내부 재귀를 생략하거나 깊이 제한으로 합법 처리하지 않는다. 따라서 종료/교체는 현재 한 후보 판정이 끝날 때까지 지연될 수 있다. 측정한 작은 배치 결과를 최악 상한으로 보지 않는다.
- HintCache는 enabled·완료·실패 아님·현재 Token 일치를 모두 만족해야 수용한다. GUI의 50ms 타이머가 결과를 가져오며 작업자는 HWND·가변 Controller·UI 콜백에 접근하지 않는다. 그리기에서도 Token을 재확인한다.
- WM_CLOSE는 match 종료와 worker.cancel 뒤 창을 파괴한다. 메시지 루프가 끝나면 View 소멸과 함께 worker가 정지 플래그·세대 변경·notify·join을 수행한다. Detached thread가 없다. 현재 코드에서 GUI 파괴 뒤 작업자가 GUI를 호출할 경로는 없다.

자동 검증은 부분 취소의 상태 보존·미완료 결과 거부, stale 세션/revision 캐시 거부, 요청 교체, 40회 취소/재요청/대기 또는 실행 중 소멸을 포함한다. 실제 사고 중 닫기·장시간 자원 누수·최악 재귀 종료 대기는 미검증이다. 시계·AI 프로세스 관리로 이 계약을 확대 해석하지 않는다.
