# 프로젝트 기반 검증 기록

검증일: 2026-10-19. 범위는 프로젝트 기반과 기본 GUI 창이다.

이 문서는 기반 단계 당시의 기록이다. 이후 상태 모델·CTest를 추가했으므로 현재 상태와 최신 실제 검증은 [상태 검증 기록](verification-state.md)·[인계](handoff.md)를 따른다. 아래의 “미구현/테스트 없음”은 기반 검증 당시를 가리킨다.

## 실제 환경
- Windows API가 보고한 버전: Microsoft Windows NT 10.0.26200.0, x64 도구 체인.
- GCC 13.3.0 / MinGW-w64 11.0.1, CMake 3.29.3, Ninja 1.12.1.
- 개발 도구 경로: C:\Program Files\mingw64\bin.
- AGENTS.md와 참조 docs 두 파일이 존재함을 읽어서 확인. 누락 문서는 없었다.

## 실행한 검증과 결과
| 항목 | 방법 | 실제 결과 |
|---|---|---|
| CMake 구성·컴파일·링크 | README의 powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\build.ps1 | 최종 수정 후 연속 두 번 성공. 출력된 컴파일 경고 없음 |
| 한글·공백 경로 | 원래 프로젝트 폴더를 유지, ASCII 임시 junction·상대 리소스 컴파일 사용 | 성공. 스크립트는 생성된 Ninja 메타데이터 두 파일을 정리하며 소스를 삭제·이동하지 않음 |
| 기본 창 실행 | build/native-release/bin/omok.exe 실제 실행, computer-use 창·화면 확인 | 제목 “오목 프로젝트”, 기본 안내 한글 표시 확인 |
| 배포 폴더 실행 | dist/omok/omok.exe 실행 | 해당 프로세스의 같은 제목 창 생성 확인 |
| 정상 창 닫기 | 배포 창의 접근성 트리에서 닫기 단추 실행 | 창 목록에서 해당 창이 사라짐을 확인. 프로세스 종료 코드 자체는 수집하지 않음 |
| 설치 스테이징 | cmake --install을 빌드 스크립트에서 실행 | exe·실행용 README·외부 고지·라이선스 세 파일 생성 |
| ZIP | README의 Compress-Archive 명령 후 항목 검사 | 6개 파일 포함, 엔진·매니저·모델·빌드 도구 미포함 |
| 실행 파일 형식 | objdump -p | PE32+ x64, Windows GUI subsystem |
| DLL 의존성 | objdump -p의 import 목록 | GDI32.dll, KERNEL32.dll, msvcrt.dll, USER32.dll만 확인. libstdc++/libgcc/winpthread DLL 추가 의존 없음 |
| 프리셋 | cmake --list-presets | mingw-release·msvc-release 인식 |

최종 산출물:
- dist/omok/omok.exe: 68,310 bytes. SHA256: 6A07271C31CC40581D59607A549DBCBE3E0CEA4965DAEA728961110AA2A71DC4
- dist/omok-0.1.0-windows-x64.zip: 36,949 bytes. SHA256: F50C13AA80D509041F325E555051A3A0F6CCC19699C7ABCBDA927CD2C70D99A8

소스와 실행 파일은 현재 작업 폴더에 있다. build·dist는 Git에서 제외한다. 타임스탬프가 바뀌는 재빌드에서는 위 해시가 달라질 수 있다.

## 발견·수정한 문제
- 최초 windres 빌드는 한글·공백 절대 include 경로를 잘못 처리했다. 독립 리소스 파일을 상대 경로에서 컴파일하도록 수정했다.
- Ninja는 기존 실패 경로가 남은 .ninja_deps에서 FindFirstFileExA 오류를 냈다. ASCII junction과 해당 빌드 디렉터리의 .ninja_deps·.ninja_log만 정리하는 절차로 재현 가능한 빌드를 확인했다. 소스나 사용자 자료를 재귀 삭제하지 않는다.
- 테스트 창 실행 중 재링크는 Windows 파일 잠금으로 실패했다. 직접 실행한 테스트 프로세스를 정리한 뒤 빌드했다. 사용자는 재빌드 전 창을 닫아야 하며 빌드 스크립트는 앱을 강제 종료하지 않는다.
- 첫 창의 최대화 시도 이후 도구가 window is minimized / user input was detected를 반환해 해당 조작 결과를 완료로 기록하지 않았다. 별도 배포 창의 닫기 단추는 마지막에 성공했으므로 정상 닫기 확인과 최대화 미검증을 구분한다. 추가 화면 캡처의 신뢰가 부족한 것은 제품 표시 확인으로 사용하지 않았다.

## 미검증·범위 제외
- 개발 도구가 없는 별도 Windows 10·11 x64 환경에서의 실제 실행은 미검증이다. DLL 목록 검사만으로 대상 환경 실행을 보장하지 않는다.
- MSVC 2022 빌드, 복수 모니터·DPI 변경·최대화/복원·장시간 GDI 자원 사용은 미검증이다.
- 게임 상태·규칙·오프닝·시계·기보·외부 AI 통신은 미구현이다. 예약 INTERFACE 타깃을 구현·테스트로 계산하지 않는다. CTest 항목이나 게임 단위 테스트는 아직 없다.
- Piskvork·GomocupJudge·Rapfi·Pela는 문서·소스 조사만 수행했다. 실행·연동·실대국 검증은 하지 않았다.
- 실제 대회 연도·리그, 200수 경계·심판 예외·대회 오류 결과·자원 강제 처리는 후속 조사·실행 검증이 필요하다.

## 다음 작업
다음 범위를 정할 때 매니저·엔진 검증 계획과 상태·렌주 사례 명세를 먼저 구체화한다. GUI와 독립된 상태 모델·규칙부터 단계별 구현하고, 외부 AI 통신은 별도 단계에서 시작한다. 이번 작업으로 자체 AI 알고리즘 개발을 시작하지 않는다.
