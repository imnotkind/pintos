#include "vm/frame.h"

struct list frame_list;

void init_frame_table()
{
    list_init(frame_list);
}

