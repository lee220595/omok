# 오목 프로젝트 개발 인계

갱신2026-10-29. **이번 승인 범위는 두 감시자 기반 학습·평가 설계의 문서/계약 갱신**이다. 감시자 코드·RL 업데이트·새 학습/평가 실행은 하지 않았다. 기존 실행/중단 승인 경계를 유지하고 다음 구현도 자동 시작하지 않는다. 직전 [PUCT 인계](handoff-before-monitoring-2026-10-29.md)를 원문 그대로 보존했다.

## 실제 기준과 사용자 설명 대조

기준 HEAD는 `2dc4eabc46947b42dba5a4cfd3de51818fd5a263`, 브랜치는 `codex/puct-search`다. 시작 시 추적/미추적 변경 없었고 fetch 후 원격 작업 브랜치와 앞/뒤0을 확인했다. main/origin/main은 `a15a871491b74c1685d1dad072c9edb392abc6f7`이다. 미통합 SL/PUCT 의존 이력을 보존해 같은 작업 브랜치에서 문서만 수정한다. main 통합/강제 푸시/브랜치 삭제/기존 변경 폐기는 하지 않는다.

사용자가 언급한 ‘handoff(9)’의 핵심인 **SL·추론·순차 PUCT 구현, RL 업데이트 미구현**은 현재 docs/handoff.md와 실제 코드에 일치한다. 별도 번호9 파일을 확보/검증했다고 주장하지 않는다. 예전 중단한 최초 AI 작업 이후 다른 작업에서 무화면/SL/PUCT 구현이 추가됐으므로 오래된 ‘.venv 생성까지만’ 상태를 현황으로 쓰지 않는다.

## 재사용할 실제 코드와 부족한 부분

| 영역 | 실제 구현/재사용 | 아직 없는 것/검증 한계 |
|---|---|---|
| C++ 규칙/대국·Python 환경 | rules::evaluate/match::place, src/headless/bridge.cpp의 mask/step, Environment snapshot/mask/observation/clone/Token | Python 독자 판정 불필요. 공통 감시 이벤트/마스킹 전 불법 질량 기록은 없음 |
| 모델/SL/체크포인트 | PolicyValueNet·checked_outputs/masked_logits, train의 loss/epoch metrics, checkpoint.load/atomic_save/initialize_for_rl | heartbeat/step 정체·gradient/optimizer 종합 감시, RL stage 저장/재개/update 미구현 |
| 순차 PUCT | mcts.search의 clone·전체 합법 후보·actor backup/실제 종료·고정 S·root 방문/NN/terminal 통계, ModelPlayer.evaluate 재사용 | 고정 시간/hard deadline·탐험/경험/RL 목표 생성·감시 실행기 없음 |
| 대국/기록/재개 | runner/records의 atomic JSON trace·독립 재생·manifest/완료 건너뛰기·미완료 attempt 보존 | 동시 output 공유/중간 대국·검색 복구 미지원. 재생은 탐색 메타데이터 전체를 검사하지 않음 |
| 비교/회귀 | compare_search의 같은 모델 policy/PUCT·흑백 교대·일부 예산 검사, 기존 CTest/test_headless/test_learning/test_mcts | 검토된 실제 CNN 전술 세트·CI/통계 승격·고정 후보 등록·중복 평가 통합 감시 미구현 |

현재 PUCT의 성공 정의는 root expansion 제외 S회 완료 순회, root 방문 합=S, NN+terminal_hits=S+1, clone=S+1이다. terminal_hits는 고유 종료 국면 수가 아니라 NN을 생략한 leaf 도달 횟수다. transitions는 경로 전체 착수 합이다. 실패 중 부분 통계에 성공 등식을 강제하지 않는다. 기존 정의를 바꾸지 않았다.

확인한 문서상의 오래된 요약 차이는 AGENTS의 ‘무화면 인터페이스 계획/AI 대 AI·기보 모두 미구현’과 개발 계획 표의 ‘MCTS 미구현’이었다. 실제 Python 환경/순차 모델 대국/JSON trace·PUCT 구현에 맞게 갱신하고 GUI AI 대 AI/범용 기보·제출 수신기/전체 평가 미완료와 분리했다. 실제 코드 변경은 없다.

## 문서 변경

- [AGENTS](../AGENTS.md)에 두 감시자/보상 책임·동일 환경/Token·독립 정답·대응 한도·고정 평가 원칙을 지속 지침으로 추가했다.
- [감시 계약](monitoring-contract.md)을 새로 작성했다. A 규칙 심판은 C++ 판정/마스크/회귀를 재사용하고 B는 모델/수치/진행·전술/대국/비용·기록/체크포인트를 관찰한다. 별도 AI 모델 두 개/분산 서비스가 아니다. 13개 규칙에 ID/버전·시점/입력·위반·심각도/대상·대응·증거·수용 검증을 정의했다.
- [보상 계약](reward-contract.md)을 새로 작성했다. 승+1/패·금수패-1/규칙 draw0의 결과 매핑 권장과 기존 board-outcome-v1 가치 자격 보류를 구분한다. 임의 -100·자동 LR/보상 변경은 없다. 후속 pi=root 방문/S, actor 관점 z·SL 초기화/RL 재개 연결 조건을 명시했다.
- [평가](evaluation-contract.md)에 규칙/실행/전술/대국/처리량 분리·전술 문제 필드·기보 재생/메타데이터 검사/탐색 재실행·고정 후보/평가 ID·흑백 쌍/사전 예산/표본·완료 단위 재개와 중복 방지를 구체화했다. 기존 관측 시간/소표본을 새 기준 통과로 소급하지 않는다.
- [환경](headless-environment-contract.md)·[모델](model-contract.md)·[데이터](data-contract.md)·[학습/평가 안내](learning-and-evaluation.md)·[PUCT](puct-search.md)·[개발 계획](development-plan.md)·[결정 목록](requirements-pending.md)·README를 연결/최신화했다. 세부 규칙은 감시 계약, 보상 자격은 보상/데이터, 통계/전술은 평가 계약에 모았다.

## 핵심 계약과 미정 사항

형식/권한/Token/종료 후 거부는 전체 보존, 금수는 시도 돌/성공 이력 불변이지만 반칙패/종료 이벤트/revision은 변경한다. 마스킹 전 확률 질량은 모델 진단이며 실제 불법 착수가 아니다. 마스킹 후 불법은 변환/마스크/상태/규칙의 구현 오류로 조사한다. 정확한 흑5목과 준비 포함200수의 기존 우선순위·blocked/aborted를 정상 결과로 바꾸지 않는 정책을 유지한다.

현재 value_target은 금수/기권 종료에 None이다. 권장 금수패 결과 점수-1을 RL 가치에 편입하려면 별도 버전/자격·오류 격리 정책을 정해야 하며 이번에 SL 동작을 변경하지 않았다. 현재 batch는 action/distribution 목표를 표현하지만 PUCT 방문 분포를 검증해 경험으로 변환하는 생성기/RL 업데이트는 없다. initialize_for_rl은 초기 가중치만 제공하며 RL 같은 단계 재개가 아니다.

정상/주의/실패/판단 보류와 명백한 불변조건을 정의했다. heartbeat 주기/진행 정체 창·지연/성능 하락 허용 폭·전술 세트/필수 모드·최소 평가 표본/승격 임계값은 측정 후 실행 전 결정한다. 임의 숫자를 운영 기본값으로 활성화하지 않는다. 표본/기준이 없으면 보류한다. 모델 크기/목표 연도·리그·장비/실제 자료 권리 등의 기존 미정도 유지한다.

자동 대응은 기록/경고/격리/승격 차단/안전 중단 요청까지다. 현재 heartbeat producer/stop hook은 없고 요청이 저장 완료/실제 종료를 뜻하지 않는다. 강제 종료·자동 재시작/재학습·보상/LR 변경은 하지 않는다. SL은 마지막 저장 epoch, 대국은 저장 완료 대국으로 복구한다. 공유 output/동시 환경 호출 미지원과 강제 종료 시 진행 대국·미완료 epoch 유실 가능성도 남긴다.

atomic_save는 파일 하나의 저장이지 mutable best.pt/latest.pt와 metrics 전체의 트랜잭션·불변 후보 등록이 아니다. 평가 전 고정본/해시·선정 epoch를 확인해야 한다. 기존 runner/comparison manifest는 code_version을 엄격 비교하므로 문서 커밋 뒤라도 이전 output 재개가 거부될 수 있다. 새 조건/코드에는 새 output을 쓰며 SL의 소스 해시 기준 재개와 혼동하지 않는다.

## 이번 검증과 과거 실행 결과 구분

이번에는 Git 상태/로그/원격·fetch/차이 확인, 실제 C++/Python 소스·테스트/계약/기존 검증 JSON을 읽고 문서만 검사했다. **빌드·CTest·학습·자동 대국·PUCT·Rapfi·GUI는 새로 실행하지 않았다.** 문서만 변경했으므로 불필요한 전체 빌드/학습을 반복하지 않았다. 감시/보상/전술 실행기는 아직 없으며 동작 검증 완료로 표시하지 않는다.

이전 실제 빌드/install·CTest91개·학습 선택 unittest6개, 통제 PUCT8개/실제 CNN 연결·12판234수/예산1·8·32 비교는 [보존 인계](handoff-before-monitoring-2026-10-29.md)·[PUCT 검증 JSON](verification/puct-search.json)의 당시 기록이다. SL의 자체10판95수·체크포인트/재개·기능 검증도 [SL 기록](supervised-learning.md)을 따른다. 고수 기보/실력 향상·광범위 전술·하드deadline/장기 대회 적합성은 미검증이다.

문서 검사 명령/결과는 아래에 기록한다. 외부 공고/라이선스·독립 심판을 새로 조사/실행하지 않았으며 기존 근거를 연결했다.

### 최종 문서 검사 결과

- PowerShell에서 저장소 Markdown44개(새 파일 포함)의 상대 링크410개를 검사해 누락0을 확인했다. 외부 URL 최신 유효성을 검증한 것은 아니다.
- 감시 규칙 행13개/고유 ID13개, 각 행의7개 필드(ID/버전 포함)와 표 구조 누락0을 확인했다. 규칙별 수용 검증은 설계이며 실행하지 않았다.
- 변경14개 모두 Markdown이고 코드/테스트/설정·기존 verification/과거 인계 변경0이다. 새 보존 인계가 HEAD의 직전 handoff와 같은 원문임을 줄 단위로 비교했다.
- 한자/가나 오타 검색0·다음 작업/미구현/보상 자격의 상충 표현을 검사했고 git diff --check를 통과했다. 기존 source와 현재 설계의 통계/호환성/재개 범위를 대조했다.

## 다음 작업 하나·저장 상태

다음 제안은 **기존 검사 결과의 순차 감시 이벤트/보고 최소 연결**이다. 계약별 정상/주의/실패/보류와 오류 주입·고정 저장 기록을 검증하고 기존 회귀를 재사용한다. 운영 임계값 활성화·새 학습/RL·대규모 평가·GUI·강제 종료는 제외한다. 목적/선행 조건/산출물/완료 기준은 개발 계획에 기록했다. 이번에는 자동 착수하지 않았다.

검증한 관련 Markdown만 같은 브랜치에 작업 단위로 커밋·일반 푸시한다. 최종 커밋 ID/푸시 결과는 완료 보고에 남긴다. 코드/테스트/설정·기존 verification JSON/과거 인계 파일·학습 데이터·checkpoint·외부 Rapfi 설치는 변경하지 않는다. 새 보존 인계는 직전 원문 사본이다. main 반영은 수행하지 않는다.
