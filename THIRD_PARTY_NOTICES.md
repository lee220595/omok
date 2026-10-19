# 배포 구성과 외부 라이선스

현재 GUI는 자체 작성 C++ 코드와 Windows 기본 User32/GDI32 API를 사용한다. Qt·wxWidgets, 매니저·AI 코드·모델·바이너리는 포함하지 않는다. Windows의 DLL은 OS 제공 구성요소이며 이 패키지에 복사·재배포하지 않는다. Windows 사용 조건은 대상 OS에 따른다.

MinGW 빌드는 GCC 런타임을 정적으로 링크한다. GCC 13.3.0, libgcc 및 libstdc++에는 GPL과 GCC Runtime Library Exception 3.1이 적용된다. 예외 조건을 충족하는 일반적인 GCC 컴파일 결과는 독립 애플리케이션의 배포 조건을 사용할 수 있다. GCC 자체나 수정한 런타임을 배포할 때의 의무와 구분한다.

- [GCC 라이선스 설명](https://gcc.gnu.org/onlinedocs/libstdc++/manual/license.html)
- [GPL 원문](licenses/GPL-3.0.txt)
- [GCC Runtime Library Exception 원문](licenses/GCC-RUNTIME-LIBRARY-EXCEPTION-3.1.txt)

MinGW-w64 11.0.1 런타임·헤더는 파일별 Public Domain/BSD/LGPL 등 예외와 기본 ZPL 2.1 조건을 확인해야 한다. 함께 제공하는 [원문](licenses/mingw-w64-COPYING.txt)의 저작권·조건·면책 고지를 유지한다. Windows 기본 DLL에 대한 import library를 OS DLL 재배포 권한으로 해석하지 않는다.

빌드 전용 CMake 3.29.3은 BSD 3-Clause 계열 고지, Ninja 1.12.1은 Apache-2.0 조건이다. 빌드 도구는 실행 패키지에 넣지 않는다. 도구를 별도 재배포한다면 각 원문·NOTICE 조건을 추가로 충족한다.

- [CMake 저작권 원문](https://github.com/Kitware/CMake/blob/v3.29.3/Copyright.txt)
- [Ninja 원문](https://github.com/ninja-build/ninja/blob/v1.12.1/COPYING)

프로젝트 자체 코드의 외부 공개 라이선스는 아직 지정하지 않았다. 이 문서는 외부 구성요소의 고지이며 자체 코드에 임의로 GPL·MIT 등의 라이선스를 부여하지 않는다. 현재 패키지는 개발용 기본 창이며 Gomocup 제출 엔진이 아니다. 매니저·엔진을 향후 동봉하거나 코드를 가져오면 해당 버전의 라이선스·소스 제공·모델 조건을 먼저 확인하고 이 문서를 갱신한다.
