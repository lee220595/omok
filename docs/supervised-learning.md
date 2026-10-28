# 작은 정책·가치 모델 지도학습 (2026-10-28)

이번 구현은 **학습 코드·저장/재개·합법 대국의 기능 검증**이다. 실제 고수 기보 지도학습·실력 향상은 미검증이다. MCTS/RL 업데이트·Gomocup 수신/패키징·GUI 연결·Rapfi 학습/평가를 구현하거나 실행하지 않았다. 기존 [환경](python-headless.md)과 [모델](model-contract.md)·[데이터](data-contract.md) 계약을 재사용한다.

## 선택 환경과 의존성

Windows11 x64 10.0.26200, Python3.13.7 x64, **PyTorch2.14.1+cpu**, Intel Core Ultra5 226V에서 CPU 검증했다. 학습/추론은 CPU 한 스레드·torch deterministic algorithms를 사용한다. Linux/Colab/MSVC/GPU·다른 버전은 미검증이다. C++/GUI 빌드는 기존 GCC13.3.0/CMake3.29.3/Ninja1.12.1을 유지하며 학습 패키지를 요구하지 않는다.

PyTorch CPU wheel을 프로젝트의 Git 제외 build/learning-venv에 설치했다. [공식 설치 안내](https://docs.pytorch.org/get-started/locally/)와 [라이선스](https://github.com/pytorch/pytorch/blob/main/LICENSE)를 확인했다. BSD3-Clause 조건의 PyTorch 및 전이 의존성은 사용자 학습 환경에 설치하며 GUI/제출 패키지에 재배포하지 않는다. 설치한 전이 버전은 검증 JSON에 남긴다. 이번은 PyTorch 추론 기능 검증이며 최종 제출용 추론 런타임 선정이 아니다.

프로젝트 루트 PowerShell에서 실행한다. Windows sandbox의 임시 파일 문제 때문에 검증 명령은 writable TEMP/TMP를 명시했다. 초기 venv ensurepip 실패는 이 설정 후 정상 생성했다. NumPy를 설치하지 않았기 때문에 torch import에서 NumPy 초기화 경고가 발생하지만 이 구현은 list/torch tensor만 사용하고 numpy API를 호출하지 않는다. 아래 학습·테스트·대국은 경고가 있는 상태에서 성공했다.

```powershell
New-Item -ItemType Directory -Force build/pip-temp | Out-Null
$env:TEMP=(Resolve-Path build/pip-temp).Path
$env:TMP=$env:TEMP
python -m venv build/learning-venv
build/learning-venv/Scripts/python.exe -m pip install -r requirements-learning.txt
$env:PYTHONPATH='python'
```

실제 첫 설치는 `pip install torch --index-url https://download.pytorch.org/whl/cpu`로 2.14.1+cpu를 설치했고, 고정한 requirements-learning.txt로 재확인했다. 공식 CPU index 네트워크 연결은 sandbox 밖에서 실행했다. 대용량 wheel·가상환경·체크포인트는 Git에 넣지 않는다.

## 모듈과 모델

python/omok_learning의 contracts는 계약/D4, data는 권리 게이트/재생/변환/분할, fixtures는 자체 검증 데이터, model은 공통 정책·가치 CNN, train은 SL/검증·선택, checkpoint는 저장/로드/SL재개·RL초기 가중치, player는 추론, evaluate는 개발 대국·별도 최종 fixture 지표를 담당한다. 기존 omok_env.runner에는 select(env,mask)/descriptor의 작은 플레이어 경계만 추가했다. 기본 플레이어에서는 torch를 import하지 않는다. C++ 판정 코드는 변경하지 않았다.

기본 모델 small-cnn-v1은 **Conv3×3(4→8)·ReLU → Conv3×3(8→8)·ReLU**의 공유 특징, **Conv1×1(8→1)** 정책 head, **전역 평균 pooling→Linear8→16·ReLU→Linear16→1·tanh** 가치 head다. 총 **1,050개 파라미터**다. 작은 CPU 경로 검증을 우선해 BatchNorm/Dropout/ResNet·큰 모델 탐색을 넣지 않았다. 구조·손실·학습률 등은 [설정](../configs/small-sl-cpu.json)으로 관리한다.

입력은 기존 renju15-observation-v1 float32 NCHW [N,4,15,15]다. 내 돌/상대 돌·현재 흑 여부·준비 포함 남은200수 비율을 그대로 쓴다. 돌을 제거하지 않으므로 수순 n은 점유 수로 도출할 수 있으나 기존 계약의 명시적 남은 수 채널을 유지하고 추가 채널은 만들지 않았다. 행동은 row*15+column, row/column0..14, 원점 왼쪽 위, A1=0/O15=224다. 출력은 logits[N,225]/가치[N,1]이며 유한성·형상·[-1,1]를 검사한다.

합법 마스크는 C++ rules::evaluate만 사용한다. 불법 logits를 -inf로 만든 뒤 안정적인 softmax/로그 softmax를 적용한다. 정책 손실은 합법 logits 기반 교차 엔트로피, 가치 손실은 현재 착수자 기준 MSE, 기본 가중치는 각각1이다. 미래 MCTS의 distribution225 target도 같은 손실 경계가 표현·검증하지만 이번에 MCTS target을 생성하지 않는다. 불법 질량·정규화/형상/NaN 오류는 거부한다. 가치 미적격 샘플은 가치 손실/지표의 분모에서 제외한다. 종료 상태는 모델 호출 금지, 합법 수0은 blocked이며 draw/임의 착수를 만들지 않는다.

학습은 train()/gradient 활성화, 검증·추론은 eval()/inference_mode()다. 정책 top1/top5는 합법 logits의 순위와 정답(분포이면 최대질량 행동)을 비교한다. 학습 후 원본 train/validation에서 각각 정책 CE·가치 MSE·사용 수와 정확도를 보고한다. 검증 weighted loss만으로 best.pt를 선택하며 final split은 train 함수에서 지표/선택에 사용하지 않는다.

## 데이터 출처·분리·변환

저장소와 runs를 확인했다. 고수 기보·외부의 권리 확인 기보는 제공되지 않았다. 이전 ordered/random 대국 및 자체 금수 fixture는 강한 교사의 자료가 아니다. 이번에는 짧고 재생 가능한 자체 합법 대국10판으로 기능을 검증했다. 흑/백 각각5판의 5목 종료, 9~10수/판, 총95수다. 상대의 멀리 떨어진 수와 일렬 승리 수를 만든 **의도적으로 쉬운 편향 fixture**이며 실제 전략 학습 데이터가 아니다.

```powershell
build/learning-venv/Scripts/python.exe -m omok_learning.fixtures runs/sl-fixtures
build/learning-venv/Scripts/python.exe -m omok_learning.data runs/sl-fixtures/sources.json runs/sl-release-dataset
```

원본은 raw trace, normalized/해시.json은 검증된 전체 대국, dataset.json은 착수 전 관측/마스크/정책/가치 샘플이다. 준비 돌은 초기 상태로만 복원하고 일반 착수 정답으로 만들지 않는다. 모든 입력은 기존 replay로 검증한 후 다시 C++ 환경에서 착수 전 관측·마스크를 얻는다. 원본의 색/actor/결과를 보존하며 winner와 그 국면 actor의 비교로 ±1을 만든다. 무승부만0이다.

기본 board-outcome-v1은 five/move_limit/board_full만 가치 적격이다. 지원되는 금수패는 앞선 합법 일반 수의 정책 자격과 가치 보류를 구분한다. 기권/시간패도 계약상 결과를 보존하되 가치 사용을 보류하며, 현재 trace replay에는 해당 사건 파서가 없어 변환 지원으로 주장하지 않는다. 금수 시도 자체는 정책에서 제외한다. 이번 trace 스키마에서 표현되는 오류/중단/blocked는 기본 격리하며 가치0을 넣지 않는다. 결과 불명/기권/시간패 등의 외부 기보는 아직 이 trace로 변환하는 파서가 없으므로 입력 정책을 바꾸어 억지로 읽지 않는다. 5목이 없다는 이유로 손상 판정하지 않는다.

sources.json은 omok-sources-v1이며 sources에 path(상대경로), sha256, role, rights{status,evidence,usage}를 명시한다. role은 functional-fixture 또는 supervised-authorized, 권리는 self-authored-local-verification 또는 permission-confirmed와 비어 있지 않은 증거를 요구한다. 이 필드는 사용자의 권리 확인 기록이며 로더가 법적 권리를 입증하는 기능이 아니다. Rapfi/평가·권리 불명 입력은 제외한다. 외부 데이터 다운로드는 하지 않았다.

실제 권리 확인 기보를 넣을 경로는 사용자가 선택한 로컬 디렉터리다. 현재 입력 형식은 **omok-trace-v1**만 지원한다. 위와 같은 sources.json을 작성해 `python -m omok_learning.data <sources.json> <새 출력폴더>`를 실행한다. 다른 기보 형식에는 출처/규칙/종료/오프닝을 보존하는 별도 변환기가 필요하다. 원본을 수정하지 않으며 normalized, conversion-report, manifest에 권리·계보·제외 사유를 남긴다.

전체 대국의 순서 있는 좌표/색/준비 여부를 D4 정규화해 중복 그룹을 만들고 그룹 단위로 seed20261028 분리한다. 학습6판58샘플, 검증2판18샘플, 최종2판19샘플이다. 목록·원본/그룹 해시·분할 시드·전처리·코드/네이티브 DLL 해시는 manifest에 고정했다. 증강은 분리 후 train에서만 R(r,c)=(c,14-r), F(r,c)=(r,14-c)의8변환을 관측·행동·마스크에 함께 적용한다. 가치 부호와 색은 바꾸지 않는다.

전체 대국 D4 중복 그룹 교차는0이다. 보드/관점/남은 수를 포함한 D4 정확 국면 공유는 train/validation1·train/final2·validation/final1이며, 첫4수 D4 prefix 공유는 모두0이다. 빈 보드 등 공유 국면을 제거하지 않았고, 모든 유사 오프닝/근접 국면의 격리나 대회별 독립성을 보증하지 않는다. 이는 functional fixture 분할이며 엄격한 실력 평가 데이터셋 완성이 아니다. 이번 제외/손상0, 프로필은 renju15-fixed200-v1 한 종류다. 추가 필터의 권리 불명/대칭 중복 거부는 테스트에서 따로 검증했다.

## 학습·체크포인트·재개

```powershell
build/learning-venv/Scripts/python.exe -m omok_learning.train runs/sl-release-dataset/dataset.json runs/sl-release-part --epochs 8 --config configs/small-sl-cpu.json
build/learning-venv/Scripts/python.exe -m omok_learning.train runs/sl-release-dataset/dataset.json runs/sl-release-trained --epochs 12 --resume runs/sl-release-part/latest.pt --config configs/small-sl-cpu.json
build/learning-venv/Scripts/python.exe -m unittest discover -s python -p test_learning.py -v
```

epochs는 추가 횟수가 아니라 목표 총 epoch다. CPU 한 스레드·seed17, batch16, AdamW lr0.01/weight_decay0.0001, StepLR gamma0.98/epoch, D4 증강을 사용했다. metrics.json은 초기/매 epoch train·validation 정책/가치 지표, 진행·설정·runtime·코드·데이터 ID를 저장한다.

- best.pt(kind=inference): 검증으로 선택된 가중치·구조·계약·출처/학습 설정·선택 epoch/step. 옵티마이저가 없으며 재개로 사용하면 거부한다.
- latest.pt(kind=sl-resume): 마지막 **완료 epoch**의 가중치·AdamW·StepLR·Python/torch CPU RNG·sampler cursor0·epoch/step·설정·데이터/분할 ID·전처리·runtime·best weights/score/epoch·전체 지표. 같은 환경/소스 해시·설정·데이터가 일치해야 한다. 커밋 ID도 기록하되 소스 내용이 같으면 문서 커밋 차이는 허용한다. 중간 mini-batch 위치 복구는 지원하지 않는다.
- initialize_for_rl(path, optimizer_mode='reset'): 같은 PolicyValueNet 가중치와 부모 해시/전환 설정을 반환한다. 기본 optimizer=None이며 다음 RL 알고리즘이 새 optimizer/scheduler/경험을 구성한다. 'preserve'는 SL resume 파일의 AdamW 상태만 명시적으로 유지한다. RL step은0/경험 없음이고 RL 학습 함수·빈 성공 경로는 없다.

저장은 같은 디렉터리 임시 파일→flush/fsync→os.replace다. 계약/구조/키/형상·가중치 유한성·내용 해시·필수 재개 상태를 검사한다. `torch.load(..., weights_only=True, map_location='cpu')`만 사용하며 실패 시 unrestricted pickle로 재시도하지 않는다([공식 직렬화 문서](https://docs.pytorch.org/docs/stable/notes/serialization)). 임의 인터넷 모델을 신뢰하는 용도가 아니라 자체 로컬 산출물용이다. 저장 실패 시 기존 파일 보존을 주입 검사했다. 추론은 같은 schema/architecture/계약/엄격한 state_dict 호환을 지원하며 runtime 차이는 기록한다. 정확한 SL 재개는 runtime까지 일치를 요구한다.

8 epoch+재개4 epoch에서 실제48회 업데이트를 수행했다. 마지막 모델의 train 정책CE는5.39567→4.30030, 가치MSE는0.99864→0.86686이었다. 검증 합산손실이 가장 낮은 것은 **epoch11**이어서 best.pt는 epoch11 가중치다. 마지막 epoch의 훈련 손실 감소가 검증 개선이나 실력 향상의 근거는 아니다. 선택 모델의 fixture 지표/대국·해시와 재현 근거는 [검증 JSON](verification/supervised-learning.json)에 남긴다.

## 추론 대국과 별도 최종 fixture 검사

```powershell
build/learning-venv/Scripts/python.exe -m omok_learning.evaluate games runs/sl-release-trained/best.pt runs/sl-games-ordered --opponent ordered --games 2
build/learning-venv/Scripts/python.exe -m omok_learning.evaluate games runs/sl-release-trained/best.pt runs/sl-games-random --opponent random --games 2
build/learning-venv/Scripts/python.exe -m omok_learning.evaluate games runs/sl-release-trained/best.pt runs/sl-games-self --opponent self --games 2
build/learning-venv/Scripts/python.exe -m omok_learning.evaluate final runs/sl-release-trained/best.pt runs/sl-release-dataset/dataset.json runs/sl-final-evaluation.json
```

select(env,mask)는 eval/inference_mode에서 모델을 로드해 합법 정책 확률 최대를 선택한다. 동률은 가장 작은 행동 인덱스다. 상태 Token·실제 mask·출력 유한성을 검사하며 임의 행동으로 오류를 덮지 않는다. 가치 출력은 기록/분석에만 쓰며 수 선택·탐색에 사용하지 않는다. 게임 내 모델을 재사용하며 착수마다 프로세스를 새로 실행하지 않는다. 모델 식별자/설정은 기존 trace config 참가자 descriptor에, 값/확률/시간은 action.inference에 추가했다. 기존 v1 필수 의미/재생은 유지하며 모델 없이도 논리 수순을 재생할 수 있다.

개발 대국과 최종 fixture 손실 검사는 별도 명령이다. final은 best 선택을 고정한 뒤 한 번 실행했다. 공유 국면이 남은 작은 자체 fixture이므로 결과는 실력/독립 최종 대회 성능의 주장이 아니다. 흑백 교대 ordered 상대2판0승2패, random 상대2판2승0패, 동일 모델 상대2판 정상 종료(A/B각1승), 총6판116수·오류0이었다. 별도 단일 프로세스 warm100회 평균은 텐서 생성/forward/출력 검사/정규화1.39880ms, 상태/마스크 재확인 포함 선택1.93509ms다. 로드 제외·CPU한스레드·affinity고정 없음이며 대국 CLI 준비 구간 일부가 겹친 elapsed와 구분한다. 상세 시간/산출물 해시는 검증 JSON을 따른다. 기록 재생·모델 오류 분리도 확인했다. 정상 대국을 다시 실행할 때 같은 출력 폴더는 완료 기록을 건너뛰며 코드/모델 해시가 바뀌면 새 폴더를 사용해야 한다.

## 검증과 남은 범위

기존 빌드/install·전체 CTest90개를 통과했다. optional 학습 unittest5개는 D4/관점/부호·권리/그룹 분리·합법 정답·모델 출력/마스크·실제 가중치 갱신/단일 샘플 과적합·로드 동등성·정확한 연속/재개·RL 초기 가중치/선택 optimizer·저장 실패 보존·형상/버전/손상 거부·완결 모델 대국/재생·모델 로드/NaN/마스크 오류를 검사한다. 원본 환경의 규칙/좌표/복제/200수/금수 회귀는 기존 검사를 재사용했다.

남은 것은 실제 권리 확인 기보·외부 파서·라벨/엄격 분할·실제 SL 성능/실력 평가다. 대회 시계/자원·최종 제출 추론/프로토콜·GPU/다른 OS·장시간 학습·범용 외부 평가·MCTS/RL은 미구현/미검증이다. 다음 작업 하나는 **정책·가치 모델에 MCTS를 연결**하는 단계이며 이번에 자동 시작하지 않는다.
