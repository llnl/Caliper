// --- Caliper continuous integration test app: threads

#include "caliper/cali.h"
#include "caliper/Caliper.h"

#include "../../src/caliper/RuntimeConfig.h"

#include <pthread.h>

cali_id_t thread_attr_id = CALI_INV_ID;

void* thread_proc(void* arg)
{
    CALI_CXX_MARK_FUNCTION;

    int thread_id = *(static_cast<int*>(arg));
    cali_set_int(thread_attr_id, thread_id);

    return NULL;
}

int main()
{
    CALI_CXX_MARK_FUNCTION;

    thread_attr_id = cali_create_attribute("my_thread_id", CALI_TYPE_INT, CALI_ATTR_SCOPE_THREAD | CALI_ATTR_UNALIGNED);

    int       thread_ids[4] = { 16, 25, 36, 49 };
    pthread_t thread[4];

    cali::Annotation("local", CALI_ATTR_SCOPE_THREAD | CALI_ATTR_UNALIGNED).set(99);
    cali::Annotation("global", CALI_ATTR_SCOPE_PROCESS | CALI_ATTR_UNALIGNED).set(999);

    for (int i = 0; i < 4; ++i)
        pthread_create(&thread[i], NULL, thread_proc, &thread_ids[i]);

    for (int i = 0; i < 4; ++i)
        pthread_join(thread[i], NULL);
}
