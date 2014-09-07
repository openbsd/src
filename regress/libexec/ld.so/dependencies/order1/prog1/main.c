#include <dlfcn.h>
#include <stdio.h>

int
main()
{
        void *libaa, *libbb;
	int flag = RTLD_NOW;

        if ((libaa = dlopen("libaa.so", flag)) == NULL) {
                printf("dlopen(\"libaa.so\", %d) FAILED\n", flag);
                return 1;
        }

	if ((libbb = dlopen("libbb.so", flag)) == NULL) {
		printf("dlopen(\"libbb.so\", %d) FAILED\n", flag);
		return 1;
	}

        if (dlclose(libbb)) {
                printf("dlclose(libbb) FAILED\n%s\n", dlerror());
		return 1;
        }

	if ((libbb = dlopen("libbb.so", flag)) == NULL) {
		printf("dlopen(\"libbb.so\", %d) FAILED\n", flag);
		return 1;
	}

        if (dlclose(libbb)) {
                printf("dlclose(libbb) FAILED\n%s\n", dlerror());
		return 1;
        }

        if (dlclose(libaa)) {
                printf("dlclose(libaa) FAILED\n%s\n", dlerror());
		return 1;
        }

	return 0;
}

