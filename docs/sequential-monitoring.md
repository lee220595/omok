# 순차 감시 이벤트·보고 최소 연결

2026-11-02. [감시 계약](monitoring-contract.md)의 13개 ID/v1을 기존 환경·모델·PUCT·기록·체크포인트 경계에 연결했다. 새 SL/RL 업데이트·실력 실험·운영 임계값 선정은 하지 않았다. [사전 기능 검증 계획](verification/monitoring-functional-plan.json)과 [실제 검증 요약](verification/sequential-monitoring.json)을 구분한다. 기준 코드는4a1514c의 미통합 SL·PUCT 이력이며 main을 기반으로 되돌리지 않았다.

## 공통 형식과 소유권

`python/omok_monitoring/events.py`의 Monitor는 단일 순차 작성자가 소유한다. 새 빈 output 폴더를 요구하고 기본 run_id는 UUID, attempt_id는1이다. 별도 run/공유 output/분산 서비스나 외부 이벤트 수신기는 없다. 직접 run_id를 제공하면 유일성은 호출자가 보장해야 한다. 수용 시나리오들도 매번 독립 UUID를 사용한다.

events.jsonl에 이벤트를 순서대로 append·flush/fsync하고, scope 종료 시 report.json을 기존 원자 JSON 저장으로 게시한다. 이벤트 필드는 다음과 같다.

- schema=omok-monitor-event-v1, run_id/attempt_id, sequence, UTC 시각, 프로세스 내 monotonic elapsed_seconds, PID
- rule_id/rule_version=1, owner A/B, state, severity, reason, evidence, actions, persisted
- 감시 구현 ID/소스 해시·설정 해시와 context의 단계·대국/수순·Token·규칙 프로필·모델/네이티브 해시·설정·trace 참조 등 적용 가능한 정보

기본 심각도는 normal/info, warning/warning, failure/error, deferred/undetermined다. 정상은 실행한 그 검사만 의미한다. report.coverage는 항상13규칙을 열거하고 미실행은 deferred/not_executed다. 운영 기준이 없는 검사는 관측값과 이유를 보류 이벤트로 기록한다. 실패/주의를 임의 정상값으로 채우지 않는다.

report에는 규칙별/상태별 이벤트 수, 격리 대상/승격 차단 플래그, 중단/저장/범위 종료 사실, 이벤트 파일 해시와 미저장 이벤트를 포함한다. 원인 검사와 바깥 실행 경계가 각각 이벤트를 낼 수 있으므로 **이벤트 수는 독립 장애 수나 모델 오류율이 아니다.** 대용량 보드/가중치를 이벤트마다 복사하지 않고 상태/마스크 해시·Token 및 기존 trace를 연결한다. Tensor 수치 근거는 shape/dtype/유한성으로 요약하고 NaN/Inf는 JSON에서 문자열로 보존한다.

## 실제 연결과 규칙별 상태

| 규칙 | 이번 구현·검증 | 남은 범위/판단 보류 |
|---|---|---|
| GR-01 | audited_step의 snapshot/mask/Token·후처리 합법 수, runner 선택 Token 연결, 금수의 보드/성공 이력 불변·반칙패/revision/종료 이벤트 변경. ModelPlayer의 마스킹 전 질량 진단 | 마스킹 전 질량의 운영 기준 없음. 같은 C++ mask/step 일치는 연결 확인이며 독립 심판 증명이 아님 |
| GR-02 | 형식/범위·actor·Token·종료 후 거부 보존. 의도적 부정 시험은 정상, 운영 stale 차단은 주의. 거부 후 상태 변경 대역은 실패 | 외부 엔진/GUI 전체 요청 감시는 미연결 |
| GR-03 | 기존 자체 준비5목/삼삼 사례의 검토된 기대값·근거, 틀린 기대값 검출/근거 없음 보류. 기존 C++ 회귀 재실행 | 별도 심판/공식 서버 새 실행·모든 배치 증명 아님 |
| GR-04 | cap/취소/blocked/error 격리·정상 WDL 제외, aborted를 completed로 속인 입력 실패 | 보상·value_target 자격 변경 없음. 금수/기권 가치 None 유지 |
| LP-01 | 기존 checkpoint.load/checked_outputs/softmax 검증 재사용. 복사 모델의 NaN/Inf·shape·손상/파일 변경 검출, 정상 forward | 새 모델 학습·제출 런타임 검증 아님 |
| LP-02 | loss/epoch callback, 수집 수치·epoch/step의 유한성/구조 검사. 실제 readonly loss·기저장 optimizer, 합성 NaN/Inf 검사 | 실제 backward/update 중 gradient/optimizer 종합 감시·새 SL 실행은 미검증. 미수집 항목 보류 |
| LP-03 | 단계 경계 heartbeat/progress, game/PUCT/SL stop hook. 가짜 시계로 진행 정체를 멋대로 판정하지 않음 | 주기 producer·별도 프로세스 감시·정체 창/지연 기준·하드 deadline 없음 |
| LP-04 | 통제된 고정 전술의 정상·비필수 실패 주의·필수 실패 감시 판정 검사 | 실제 CNN의 검토된 필수 전술 세트/예산 관문 미확정, 없으면 보류 |
| LP-05 | train/validation/loss 지표 관측 경계와 보류 이유 기록 | 망각/과적합 판정 창·성능 근거 없음, 자동 기준 미활성 |
| LP-06 | 실행 결과 관측과 표본/승격 기준 부재 보류 | 실력 평가·유의성/승격 판정기 미구현 |
| LP-07 | 독립 보드 재생, 성공 PUCT 메타데이터 구조/방문 분포/마스크 검사, 보드/통계 변조와 저장 실패 검출 | 구조 검사/논리 재생을 탐색 재실행·모든 메타데이터 진위 증명으로 확대하지 않음 |
| LP-08 | 고정 파일 해시·kind/완료 epoch·저장 확인, 가변 모델/중복 결과 검출, 중복 완료 재수신 건너뛰기 | 전역 후보/평가 등록소·외부 이벤트 수신/분산 dedup 아님 |
| LP-09 | 성공 PUCT에만 기존 등식·분포 검사, 실행 실패와 부분 통계 구분, 비용 관측 | 시간/처리량 임계값·하락 판정 비활성. 감시 I/O 비용을 포함하므로 과거 비감시 처리량과 직접 비교하지 않음 |

모든 규칙이 모든 운영 조건까지 완료됐다는 뜻이 아니다. 실제로 검사한 범위만 정상/실패로 판단하고, LP-03/04/05/06 등의 운영 판단은 보류를 유지한다. 규칙 ID/버전·반응 한도는 원래 계약을 변경하지 않았다. 정규화 허용 오차는 기존 PUCT의1e-5를 재사용하며 새 성능 임계값을 도입하지 않았다.

## 중단·저장·격리

실패는 해당 대상의 논리 격리 목록·promotion_blocked 플래그와 안전 중단 요청을 만든다. 파일을 삭제/이동하거나 모델을 고치지 않는다. 다음 순차 경계에서 실행기가 요청을 수용한다. 실행 중 보상/LR/예산 변경·강제 종료·자동 재시작/재학습은 없다.

| 사실 | 기록/의미 |
|---|---|
| 요청 | stop_requested 이벤트/플래그. 수용/저장/종료를 뜻하지 않음 |
| 수용 | boundary의 stop_accepted. game 시작/착수 전, PUCT simulation 전, SL 시작/epoch/batch 전 |
| 저장 | writer 성공 후 파일 해시로 artifact_save_confirmed. 실패는 별도 LP-07 failure/storage_failed |
| 복구 기준 | 검증된 기존 파일은 recovery_anchor_verified(new_save=false). 새 저장과 구분 |
| 범위 종료 | 소유한 순차 scope가 반환/예외 종료해 execution_scope_ended. OS 프로세스 종료 관측은 아님 |
| 보고 완료 | report.json의 원자 저장이 성공한 뒤에만 report_saved/report_save_confirmed |

safe_stop_complete는 요청+수용+scope 종료+필수 저장 근거 충족·저장 실패 없음일 때만 참이다. 이는 학습/대국 성공이나 후보 승격을 의미하지 않는다. 오류 대국도 격리된 aborted trace와 요약을 정상 저장해 안전 중단을 확인할 수 있지만 정상 WDL/경험으로 편입하지 않는다.

SL은 마지막 저장 완료 epoch만 복구 기준으로 삼는다. 모니터링 경로는 sl_recovery_checkpoint가 확인돼야 하며 metrics 파일만 있다고 checkpoint 저장을 주장하지 않는다. 미완료 epoch를 새 완료 checkpoint로 저장하지 않는다. 새 SL 실행을 금지한 이번 검증에서는 SL 진입 전 stop과 **기존 SL checkpoint 사본의 검사/저장 실패·보존**까지만 실행했다. 실제 update 중 중단/전체 SL 재개를 재실행한 것은 아니다. 기존 source/runtime 엄격 재개 조건도 바꾸지 않았다.

대국은 기존 완료 파일 단위 재개·미완료 attempt 보존을 유지한다. PUCT 중단은 성공 분포를 반환하지 않고 부분 통계를 보류한다. 실패 검색의 부분 통계에 `root N=S`·`NN+terminal=S+1` 등 성공 등식을 적용하지 않는다. heartbeat는 경계 도달 증거이며, 오래 걸리는 native/forward 호출 중에는 갱신할 수 없다. 주기/정체/프로세스 생존 보장은 없다.

이벤트 파일 저장 실패는 메모리의 persisted=false 이벤트와 예외로 남기고, 이후 보고 저장이 가능하면 unpersisted_events에 담는다. 보고서 저장까지 불가능하면 보고 파일이 없고 MonitoringStorageError가 발생하며 안전 완료를 주장하지 않는다. 전원 손실/디스크 자체 장애의 완전한 내구성은 보증하지 않는다.

## 실제 실행과 제한된 수용 검증

실행 전에 [고정 계획](verification/monitoring-functional-plan.json)을 남겼고 각 acceptance output의 plan.json도 환경 생성 전에 작성한다. 새 통합 대국은 자체 준비 배치/일반 착수 최대1수, PUCT 최대2 simulation, 모델 검사는 실제 착수0수다. 통제 평가기/가짜 시계/임시 디렉터리·checkpoint 사본만 사용한다. optimizer update0/RL update0, source checkpoint 해시는 전후 일치했다.

```powershell
$env:PYTHONPATH='python'
python -m omok_monitoring.acceptance runs/monitor-v1-verified
$env:TEMP=(Resolve-Path build/pip-temp).Path
$env:TMP=$env:TEMP
build/learning-venv/Scripts/python.exe -m omok_monitoring.neural_acceptance runs/sl-release-trained/best.pt runs/sl-release-trained/latest.pt runs/monitor-v1-neural
python -m unittest discover -s python -p test_monitoring.py -v
build/learning-venv/Scripts/python.exe -m unittest discover -s python -p test_monitoring_neural.py -v
build/learning-venv/Scripts/python.exe -m unittest -v test_learning.LearningTests.test_puct_real_model_load_once_eval_mode_and_reproducibility
```

25개 시나리오(순수16+선택 neural9), 이벤트319개를 검증했다. 정상241·주의2·의도한 실패23·보류53이며, 기대된 오류 주입이 검출된 결과이지 수용 테스트23개 실패가 아니다. 새 unittest6개+선택 neural1개 통과. 기존 무업데이트 PUCT 모델 연결1개와 CTest91개도 통과했다. SL 학습/재개·과적합 테스트처럼 optimizer update를 하는 항목은 이번 승인 범위 때문에 실행하지 않았다. 선택된 기존 학습 테스트의 setup은 자체6판·각 최대10수 fixture를 임시 생성할 뿐 학습하지 않는다.

CTest의 초기 시도는 삭제된 과거 ASCII 임시 별칭을 참조해 실패/미실행이었다. C++ 소스 변경 없이 기존 스크립트로 캐시/별칭·빌드를 재생성한 뒤91개 통과(10.61초)했다.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent C:/Users/leesn/AppData/Local/Temp/omok-monitor-build -Headless
ctest --test-dir build/native-release --output-on-failure --no-tests=error
python -m omok_env.runner runs/monitor-cli-game --games 1 --seed 23 --players ordered ordered --compact a8a1b8c1c8e1d8g1 --max-actions 1 --monitor-output runs/monitor-cli-events
```

CLI 연결은1수 뒤 cap/interrupted1·winner 없음으로 저장됐다. 저장 실패·PUCT1회 진행 후 취소·잘못된Token/종료 후 요청·금수·NaN/Inf/shape·가변 모델/통계/보드 변조 등 상세 근거·시나리오별 중단 상태는 검증 JSON에 있다. output은 독립 빈 경로를 사용하며 위 결과가 이미 있으면 새 폴더로 실행한다. Windows11/Python3.13.7/기존 torch2.14.1+cpu에서 검증했고 새 패키지를 설치하지 않았다. 기존 NumPy 미설치 경고는 남지만 해당 API를 사용하지 않는다.

## 다음 단계의 조건

PUCT 방문 분포 기반 경험 생성/RL 업데이트 **설계에는 진입 가능**하다. 실행/학습을 시작했다는 뜻은 아니다. 성공한 고정 모델 검색의225분포·Token/actor/프로필·완료 대국 ID와 계보, 정상 종료 자격/오류 격리·데이터 분할, 탐험/온도/노이즈·예산·경험 재사용, 별도 RL stage/kind·optimizer/RNG/경험 cursor 재개와 감시 소비 경계를 먼저 정해야 한다. 실제 update 중 수치/중단·복구 검증도 별도 필요하다.

[보상 계약](reward-contract.md)의 현재 board-outcome-v1/value_target은 변경하지 않았다. 금수패·기권의 가치 목표 편입은 새 버전/자격 정책이 필요한 미해결 사항이다. 이를 해결하지 않아도 제외/보류하는 설계는 가능하나 자동 포함하지 않는다. 실제 CNN 전술/성능·승격 기준도 운영 전에 확정해야 한다. 이번에는 경험 생성/RL·실력 실험을 자동 시작하지 않는다.
