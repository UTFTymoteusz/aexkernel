#pragma once

void yield() {
    task_switch_full();
}