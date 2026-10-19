# 매니저·외부 AI 후보와 호환성 결정

조사일 2026-10-19. 공식 제작자 저장소·문서를 사용했다. 원격 실행 파일·엔진·모델은 설치·실행하지 않았다. 일부 소스는 build/research에 조사 캐시로만 내려받았으며 제품이나 배포물에 포함하지 않는다.

## 소스 기준점
| 저장소 | 확인한 커밋 |
|---|---|
| plastovicka/Piskvork | 2c3296514a3735cad83eed8a3134f51324f962ca |
| Gomocup/GomocupJudge | 56cfd7dc1a0d1ec06d29f44a26931f375ad05991 |
| dhbloo/rapfi | 3c94c2a976f24a0dd1c5517623e9ab6fffe66bd7 |

커밋 식별자는 공식 GitHub API에서 확인했다. 후속 검증에서는 대상 바이너리가 해당 소스와 대응하는지도 확인한다.

## 매니저
| 후보 | 문서·소스 확인 | 연결·배포 판단 | 실제 검증 |
|---|---|---|---|
| Piskvork, 우선 검증 후보 | 제작자 안내에 렌주·자동 오프닝·파이프 로그·AI 대 AI 배치 실행·CPU affinity·시간·메모리 설정. protocol.cpp에 CreatePipe/CreateProcess, affinity와 프로세스 관리 코드 | GUI 내부 라이브러리로 포함하지 않고 별도 검증 도구로 먼저 사용 제안. 일반적인 별도 GUI용 제어 API는 확인 못함. protocol.cpp는 GPL-3.0-or-later 명시. 수정·코드 포함·재배포 시 GPL 의무 확인 | 미실행. 200수 제한·Swap2·심판 예외·시간/메모리 단위와 경로 인코딩은 대상 버전으로 검증 필요 |
| GomocupJudge, 대회 심판 비교 후보 | 공식 README에 client의 엔진 실행·자원 감시·심판, server의 대회 관리. client README는 실제 구성·통신 역할 설명 | Python 기반 별도 도구. 저장소 트리와 API metadata에서 명시적 LICENSE/COPYING·SPDX를 찾지 못함. 허가 없이 코드 편입·수정 재배포하지 않음. 외부 GUI 연결 계약도 미확인 | 미실행. 심판 판정·200수 경계·Windows 의존성·실행 절차 후속 검증 |

근거:
- [Piskvork 제작자 설명·라이선스·CLI](https://github.com/plastovicka/Piskvork/blob/2c3296514a3735cad83eed8a3134f51324f962ca/piskvork.txt)
- [Piskvork 프로세스·통신 소스와 GPL 표기](https://github.com/plastovicka/Piskvork/blob/2c3296514a3735cad83eed8a3134f51324f962ca/source/protocol.cpp)
- [GomocupJudge](https://github.com/Gomocup/GomocupJudge/tree/56cfd7dc1a0d1ec06d29f44a26931f375ad05991)
- [GomocupJudge client 문서](https://github.com/Gomocup/GomocupJudge/blob/56cfd7dc1a0d1ec06d29f44a26931f375ad05991/client/README.md)

Piskvork 문서의 일부 시간·메모리 CLI 설명에는 단위 표현이 서로 맞지 않는 부분이 있다. 해당 옵션을 그대로 복사해 동작하는 실행 명령으로 제공하지 않는다. 승인된 범위에서 매니저를 검증용으로 활용하되 최종 채택은 실행 결과로 보고한다.

## 외부 AI
| 후보 | 근거와 문서상 지원 | 배포 조건·제약 | 권장 용도·상태 |
|---|---|---|---|
| Rapfi, 우선 실제 엔진 후보 | 공식 README는 Gomoku/Renju와 Piskvork 호환 설명. 고정 소스 command/gomocup.cpp에 INFO rule 4의 RENJU, SWAP2BOARD 분기·처리 함수 확인 | GPL-3.0. 재배포 시 해당 바이너리의 대응 소스·라이선스 등 의무 충족. 네트워크 파일·패키지별 조건과 CPU 명령어·스레드 설정은 별도 확인 | 기능 폭을 검증하기 위한 사용자 지정 외부 프로세스. 렌주와 Swap2 지원을 결합 규칙 지원으로 해석하지 않음. 실행·연동 미검증 |
| Pela, 보조 후보 | Piskvork source/pbrain README에 pbrain-pela.exe 빌드·프로토콜 설명, piskcomp.cpp에 info_renju와 금수 검사 사용 확인 | 소스 헤더에 GNU GPL 표기. 정확한 배포 버전·부속 통신 코드의 조건을 고정해야 함. SWAP2BOARD는 확인 못함 | 기본 Gomocup·렌주 교차 검증 후보. Swap2 적합성 판단 보류. 실행·연동 미검증 |

근거:
- [Rapfi README·배포 조건](https://github.com/dhbloo/rapfi/blob/3c94c2a976f24a0dd1c5517623e9ab6fffe66bd7/Readme.md)
- [Rapfi Gomocup 처리 소스](https://github.com/dhbloo/rapfi/blob/3c94c2a976f24a0dd1c5517623e9ab6fffe66bd7/Rapfi/command/gomocup.cpp)
- [Pela 제작자 설명](https://github.com/plastovicka/Piskvork/blob/2c3296514a3735cad83eed8a3134f51324f962ca/source/pbrain/README.txt)
- [Pela 소스의 렌주 검사](https://github.com/plastovicka/Piskvork/blob/2c3296514a3735cad83eed8a3134f51324f962ca/source/pbrain/piskcomp.cpp)

외부 AI의 검색 알고리즘 코드를 가져오거나 자체 AI로 소개하지 않는다. 현재 GUI ZIP에는 매니저·외부 AI·모델을 동봉하지 않는다.

## 현재 정한 경계와 후속 검증
1. 자체 GUI와 규칙·대국 제어는 독립 모듈로 준비한다. 기존 매니저는 우선 외부 검증 역할로 제안한다. 필수 매니저 활용 요구는 유지한다.
2. 기본 규칙은 대회용 잠정 렌주다. SWAP2BOARD는 별도 경로로 구현·검증한다. 미지원 엔진에서 다른 규칙으로 자동 전환하지 않는다.
3. 프로토콜 좌표는 0부터, BOARD는 자신/상대 기준이며 렌주는 착수 순서를 보존한다. 참가자 A/B·흑/백·자신/상대 매핑을 별도 모델로 둔다.
4. GUI에 통신 대기를 넣지 않는다. 향후 비동기 통신·취소·시간 초과·종료·늦은 응답 폐기·재시작 정리를 대국 세션 단위로 구현한다.
5. 공식 프로토콜 시간 의미, 대상 심판의 200수 계산·승패 우선순위, 오류 결과·초기화 계측·메모리/affinity/pondering 제한을 실제 버전으로 확인한다. 파이프 메시지만 보내는 것으로 자원 제한을 강제했다고 보고하지 않는다.
6. 모의 AI로 형식 오류·무응답·종료·불법 수를 먼저 시험하고 Rapfi·Piskvork 후보로 실제 로그와 대국 결과를 비교한다. 불일치 시 승인된 중단·기록·결과 미확정 정책을 적용한다.

현재 기본 창의 완성을 매니저 채택·Gomocup 호환 완료·대회 규칙 일치로 표시하지 않는다. 참가 연도·리그를 선정할 때 공식 공고를 다시 확인한다.
