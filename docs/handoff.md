# 오목 프로젝트 개발 인계

2026-10-28 갱신. 이번 승인 범위는 **PyTorch 작은 정책·가치 CNN의 지도학습·저장/로드/SL 재개·RL 초기 가중치·기존 무화면 환경의 모델 추론 대국과 기록 재생**이다. 기능 검증을 완료했고 MCTS/RL 업데이트·Gomocup 프로토콜/패키징·GUI·Rapfi 학습/평가·다른 규칙은 구현/실행하지 않았다. 다음 단계는 자동 시작하지 않는다. 이전 [무화면 인계](handoff-before-sl-2026-10-28.md)와 더 과거 기록을 보존한다.

## 현재 구현과 변경

- python/omok_learning의 contracts/data/fixtures/model/train/checkpoint/player/evaluate를 추가했다. 데이터 권리 manifest/원본 해시→기존 trace 재생→C++ 환경 착수 전 관측/마스크→대국·D4 그룹 분할→샘플→CPU SL→체크포인트→모델 합법 대국·독립 재생 경로다. Python에 판정을 복제하지 않았다.
- 모델은 Conv3x3 두 층/8채널·공유 특징/정책 Conv1x1/가치 평균 pooling+MLP, 1,050파라미터다. CE+MSE/AdamW/StepLR와 선택/재개 설정은 configs/small-sl-cpu.json에 있다. SL과 미래 RL에서 같은 클래스/계약을 사용한다.
- 추론용 best.pt와 SL 재개용 latest.pt를 구분하고 계약/형상/해시/필수 상태를 검사한다. CPU·Python RNG/optimizer/scheduler/완료 epoch cursor0를 저장한다. RL 초기화는 실제 공유 가중치 로드만 하며 기본 optimizer 초기화/선택 보존을 구분한다. RL 경험/업데이트·가짜 성공 함수는 없다.
- python/omok_env/runner.py에 select(env,mask)/descriptor 경계·모델 식별자·inference 메타데이터·승자/선택시간 요약을 추가했다. 기존 ordered/random, trace v1 필수 필드/재생·재개를 보존한다. 기본 경로는 torch를 import하지 않는다. C++/CMake/GUI/외부 엔진 소스 변경 없음.
- python/test_learning.py의 선택적 unittest5개, requirements-learning.txt, .gitignore의 모델/실험 제외, README/계약/계획/결정/배포 고지·[실행 안내](supervised-learning.md)·[소형 검증 JSON](verification/supervised-learning.json)을 갱신했다.

## 실제 데이터·학습과 결과

기존 runs의 기준 플레이어 기록과 자체 fixture를 확인했으며 고수 기보·외부 권리 확인 자료는 없었다. 외부 수집 없이 자체 작성한 쉬운 합법 대국10판/95수로 기능을 검증했다. 흑/백 각각5판의 5목 종료다. 이는 고수 기보/실력 학습 데이터가 아니다.

대국·D4 중복 그룹 분리 후 train6판58샘플, validation2판18샘플, final2판19샘플이다. 전체 대국 그룹 교차0, D4 정확 국면 공유 train/val1·train/final2·val/final1, 첫4수 prefix 공유0이다. 공유 빈 보드 등은 제거하지 않고 보고했다. 모든 유사 국면/대회별 독립성/실력 평가 분할 완료를 주장하지 않는다. 준비 돌은 정답이 아니며 금수 시도·비보드 결과·실행 오류의 정책/가치 자격을 구분한다. 기권/시간패 외부 사건 파서는 현재 미지원이며 손상으로 임의 간주하지 않는다.

CPU1스레드/seed17/batch16, lr0.01·weight_decay0.0001·gamma0.98, D4 증강, 정책/가치 가중치1로 8epoch→SL 재개12epoch까지48업데이트를 실행했다. 첫8epoch1.52588초·재개4epoch0.73762초(프로세스 import/설치 제외). train CE5.39567→4.30030, valueMSE0.99864→0.86686. 검증 합산손실로 선택한 것은 epoch11/step44이며 final은 선택에 사용하지 않았다. 별도 단일 train 샘플100업데이트는 합산손실6.39081→0.00000250339·top1일치/가중치 갱신을 확인했다. 이는 코드 과적합 검사다.

모델 best SHA256 a7f9678942246028624ce694c35d16fc9d0e08b855f3fc385f4cac43b75be310, 9,733바이트. 데이터 ID e589445cc79805a3759f098c4e5c5e22fcfd5efb2fe6db95b4320f8e9fa5c6c2. 전체 분할 목록/원본·DLL·체크포인트 해시와 runtime/설정은 검증 JSON에 있다. 원시 trace/샘플·가상환경/로그/체크포인트는 runs/build에 있고 Git 제외다.

선택 모델 대국은 ordered 상대2판0승2패, random 상대2판2승0패, 동일 모델 상대2판 모두 정상 종료(A/B각1승)다. 모두 흑백 교대/기본 seed23, 총6판116수·오류0, 저장 후 독립 재생 일치. 소표본 개발 연결 점검이며 강함/Elo/개선 근거가 아니다. 각 CLI의 준비 구간 일부는 겹쳤으므로 대국 elapsed는 고립 처리량으로 해석하지 않는다. 별도 단일 프로세스 warm100회는 준비3수 상태에서 모델 forward+텐서/마스크 정규화/검사 평균1.39880ms, 환경 상태/마스크 재확인 포함 선택1.93509ms(로드 제외, affinity고정 없음)였다.

선택을 고정한 뒤 별도 final fixture19샘플 검사: CE4.33684, valueMSE1.07689, top1=0, top5=0.21053. 실제 고수 기보 SL 성능·실력 향상은 미검증이다.

## 실제 환경·명령·검증

Windows11 x64 10.0.26200, Python3.13.7 x64, PyTorch2.14.1+cpu, Intel Core Ultra5 226V/논리CPU8, GCC13.3.0 MinGW-w64/CMake3.29.3/Ninja1.12.1. CPU 한 스레드/torch deterministic algorithms다. 선택 가상환경은 build/learning-venv. PyTorch BSD3-Clause/공식 CPU index를 확인했고 GUI/제출 패키지에 동봉하지 않는다. 전이 버전은 검증 JSON에 있다.

```powershell
New-Item -ItemType Directory -Force build/pip-temp | Out-Null
$env:TEMP=(Resolve-Path build/pip-temp).Path
$env:TMP=$env:TEMP
python -m venv build/learning-venv
build/learning-venv/Scripts/python.exe -m pip install -r requirements-learning.txt
$env:PYTHONPATH='python'
build/learning-venv/Scripts/python.exe -m omok_learning.fixtures runs/sl-fixtures
build/learning-venv/Scripts/python.exe -m omok_learning.data runs/sl-fixtures/sources.json runs/sl-release-dataset
build/learning-venv/Scripts/python.exe -m omok_learning.train runs/sl-release-dataset/dataset.json runs/sl-release-part --epochs 8 --config configs/small-sl-cpu.json
build/learning-venv/Scripts/python.exe -m omok_learning.train runs/sl-release-dataset/dataset.json runs/sl-release-trained --epochs 12 --resume runs/sl-release-part/latest.pt --config configs/small-sl-cpu.json
build/learning-venv/Scripts/python.exe -m omok_learning.evaluate games runs/sl-release-trained/best.pt runs/sl-games-ordered --opponent ordered --games 2
build/learning-venv/Scripts/python.exe -m omok_learning.evaluate games runs/sl-release-trained/best.pt runs/sl-games-random --opponent random --games 2
build/learning-venv/Scripts/python.exe -m omok_learning.evaluate games runs/sl-release-trained/best.pt runs/sl-games-self --opponent self --games 2
build/learning-venv/Scripts/python.exe -m omok_learning.evaluate final runs/sl-release-trained/best.pt runs/sl-release-dataset/dataset.json runs/sl-final-evaluation.json
build/learning-venv/Scripts/python.exe -m unittest discover -s python -p test_learning.py -v
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 -AliasParent C:/Users/leesn/AppData/Local/Temp/omok-headless-build -Headless
ctest --test-dir build/native-release --output-on-failure --no-tests=error
```

기존 출력이 있으면 fixtures/data/train은 새 폴더를 사용한다. 기존 완료 대국은 같은 설정/소스/모델해시에서 건너뛴다. 첫 설치는 CPU index의 torch를 설치해 버전을 확인한 뒤 requirements로 고정/재확인했다. 초기 ensurepip/TEMP·네트워크 sandbox 실패는 writable TEMP와 승인된 네트워크 설치로 해결했다. NumPy 미설치 import 경고는 남지만 numpy API를 쓰지 않으며 모든 검증이 통과했다. 학습 저장 중 실패를 주입해 기존 체크포인트 보존도 확인했다.

- 기본 C++/GUI/DLL 빌드·install 성공. CTest90개 통과16.97초(내부 headless Python10개 포함).
- optional 학습 unittest5개 통과15.678초. 좌표/흑백·가치 부호, D4 관측/행동/마스크와 C++ 환경 대조, 권리/중복/분할·불법 정책 거부, 모델 형상/유한성·softmax, 실제 갱신/과적합·가치 미적격 손실 제외, 저장/로드·연속2epoch=1epoch+재개 가중치/optimizer/RNG/지표 일치, RL 초기 가중치/옵티마이저 선택, 손상/계약/형상 거부·저장실패 보존, 모델 완결 대국·재생·로드/NaN/마스크오류·동점 처리를 확인했다.

## 미검증·다음 작업 하나·Git

실제 고수 기보/권리 확인 데이터·외부 기보 파서/기권 등 사건·엄격 누출 제거/실력 향상, Linux/Colab/다른Python/GPU/장시간, 제출 추론 런타임·Gomocup 수신/대회 조건은 미검증/미구현이다. Rapfi/GUI 수동 검증은 재실행하지 않았다. 과거 제한은 이전 문서에 보존했다. 이번은 현재15x15/고정200수 프로필만 사용했다.

**다음 작업 하나는 정책·가치 모델에 MCTS 연결**이며 이번에 자동 시작하지 않는다. 현재 모델은 기능 검증용이라는 한계를 유지한다.

시작65037a5/codex/python-headless-games, 추적/미추적 변경 없음. fetch 후 main과 기존 의존 이력11커밋앞/뒤0을 확인했다. 미통합 기반을 보존해 codex/supervised-policy-value에서 구현했다. 기존 브랜치/이력/외부 바이너리는 보존했고 강제 푸시/삭제/main 통합은 하지 않았다. 관련 코드·설정·작은 테스트/검증·문서만 커밋·일반 푸시한다. 최종 ID/실제 푸시 결과는 완료 보고에 남긴다. 문서 상대 링크329개 누락0·Python compileall·git diff --check 통과를 확인했다. 다음 모델/탐색 작업을 자동 승인으로 해석하지 않는다.
