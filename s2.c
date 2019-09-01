#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h> 
#include <stdbool.h>
#include <pthread.h>

#include "c_api.h"

/* utilities */
static void press(int key, int mask, int button) {
    syn_press_key(key, mask, button);
    usleep(20000);
    syn_unpress_key(key, mask, button);
    usleep(20000);
}

static void click(int button) {
    syn_click_mouse(button);
    usleep(20000);
    syn_unclick_mouse(button);
    usleep(20000);
}

/* global state machine states */
enum {
    S_INIT = 0,
    S_SELECT_LEFT_P,
    S_SELECT_RIGHT_P,
    S_SELECT_CONFIRM,
    S_QUEST_START,
    S_QUEST_DONE,
    S_BM_ON,
    S_FIGHT_1,

};

/* mutex between the original Synergy thread and our own thread */
static pthread_mutex_t g_mutex;
static pthread_cond_t g_cv;

static struct {
    /* is the target system currently controlled by the user */
    bool has_focus;
    /* enum */
    int state;
    bool move_done;
    bool end_pressed;

    int left_col_x, left_col_y;
    int right_col_x, right_col_y;
    int confirm_x, confirm_y;

    time_t bm_on_time;
    time_t fight_on_time;
} g_state;

/* wait for a specific keypress from the user */
static void wait_for_confirm(void) {
    fprintf(stderr, "Waiting for input...\n");

    pthread_mutex_lock(&g_mutex);
    while(!g_state.end_pressed) {
        pthread_cond_wait(&g_cv, &g_mutex);
    }
    pthread_mutex_unlock(&g_mutex);

    g_state.end_pressed = false;
    fprintf(stderr, "Done\n");
}


static bool move_to(int dest_x, int dest_y) {
    int x, y;
    int stepX, stepY;
    int i;
    int steps = 20;

    syn_get_mouse(&x, &y);
    stepX = (dest_x - x)/steps;
    stepY = (dest_y - y)/steps;

    for (i = 0; i < steps; i++) {
        x += stepX;
        y += stepY;
        syn_mouse_move(x, y);
        usleep(40000);
    }

    syn_mouse_move(dest_x, dest_y);
    usleep(70000);
}

static void iter(void) {
    switch (g_state.state) {
        case S_INIT:
            fprintf(stderr, "init ok!\n");
            g_state.move_done = true;

            wait_for_confirm();
            syn_get_mouse(&g_state.left_col_x, &g_state.left_col_y);
            fprintf(stderr, "s_left: %d %d\n", g_state.left_col_x, g_state.left_col_y);
            wait_for_confirm();
            syn_get_mouse(&g_state.right_col_x, &g_state.right_col_y);
            fprintf(stderr, "s_right: %d %d\n", g_state.right_col_x, g_state.right_col_y);
            wait_for_confirm();
            syn_get_mouse(&g_state.confirm_x, &g_state.confirm_y);
            fprintf(stderr, "s_confirm: %d %d\n", g_state.confirm_x, g_state.confirm_y);
            g_state.state = S_QUEST_START;
            break;
        default:
            break;
    }

    if (g_state.end_pressed) {
        memset(&g_state, 0, sizeof(g_state));
        return;
    }

    if (g_state.has_focus) return; 

    time_t now = time(NULL);
    bool done = false;

    switch(g_state.state) {
        case S_QUEST_START:
            syn_enter_screen();
            syn_press_key(100, 0x2000, 0x28); //D
            usleep(80 * 1000);
            syn_unpress_key(100, 0x2000, 0x28);
            usleep(80 * 1000);
            syn_press_key(119, 0x2000, 0x19); //W
            sleep(3);
            syn_unpress_key(119, 0x2000, 0x19);
            usleep(200 * 1000);
            syn_press_key(115, 0x2000, 0x27); //S
            usleep(80 * 1000);
            syn_unpress_key(115, 0x2000, 0x27);
            usleep(300 * 1000);
            syn_press_key(97, 0x2000, 0x26); //A
            usleep(80 * 1000);
            syn_unpress_key(97, 0x2000, 0x26);
            usleep(300 * 1000);
            //press(119, 0x0, 0x19); //W
            //press(115, 0x0, 0x27); //S
            //press(97, 0x0, 0x26); //A
            //press(100, 0x0, 0x28); //D
            //usleep(500 * 1000);

            /* left pillar */
            do {
                done = move_to(g_state.left_col_x, g_state.left_col_y);
            } while (!done);
            click(1);

            do {
                done = move_to(g_state.confirm_x, g_state.confirm_y);
            } while (!done);
            click(1);

            /* right pillar */
            do {
                done = move_to(g_state.right_col_x, g_state.right_col_y);
            } while (!done);
            click(1);

            do {
                done = move_to(g_state.confirm_x, g_state.confirm_y);
            } while (!done);
            click(1);

            g_state.state = S_QUEST_DONE;
            break;
        case S_QUEST_DONE:
            syn_leave_screen();
            if (now - g_state.bm_on_time > 155) {
                g_state.state = S_BM_ON;
                break;
            }
            
            g_state.fight_on_time = time(NULL);
            g_state.state = S_FIGHT_1;
            break;
        case S_BM_ON:
            g_state.bm_on_time = time(NULL);
            g_state.fight_on_time = time(NULL);
            press(61375, 0x0, 0x44); //F2
            press(56, 0x0, 0x11); //8
            press(56, 0x0, 0x11); //8
            g_state.state = S_FIGHT_1;
            break;
        case S_FIGHT_1:
            usleep(600 * 1000);
            if (now - g_state.bm_on_time > 90) {
                press(61374, 0x0, 0x43); //F1
                press(61374, 0x0, 0x43); //F1
            }

            press(61193, 0x2000, 0x17); //TAB
            usleep(30 * 1000);
            press(49, 0x2000, 0x0A); //1
            usleep(500 * 1000);
            press(32, 0x2000, 0x41); //Space
            press(32, 0x2000, 0x41); //Space
            usleep(20 * 1000);
            press(61193, 0x2000, 0x17); //TAB
            usleep(30 * 1000);
            press(32, 0x0, 0x41); //Space
            usleep(15 * 1000);
            press(50, 0x2000, 0x0B); //2
            usleep(500 * 1000);
            press(32, 0x2000, 0x41); //Space
            press(32, 0x2000, 0x41); //Space
            usleep(13 * 1000);
//            press(61211, 0x0, 0x09); //ESC
//            press(104, 0x0, 0x2b); //H

            if (now - g_state.fight_on_time > 26) {
                press(104, 0x0, 0x2b); //H
                press(104, 0x0, 0x2b); //H
                sleep(1);
                g_state.state = S_QUEST_START;
            }
            break;
        default:
            break;
    }
    
    return;
}

static void syn_change_focus_cb(bool in) {
    fprintf(stderr, "focus hook %d\n", in);
    g_state.has_focus = in;
}

static void syn_bgthread_cb(void) {
    fprintf(stderr, "hooked!\n");
    abort();
}

static void syn_keypress_cb(int key, int mask, int button) {
    if (key == 61271) { //END
        pthread_mutex_lock(&g_mutex);
        g_state.end_pressed = true;
        pthread_cond_signal(&g_cv);
        pthread_mutex_unlock(&g_mutex);
    }
}

static void syn_exit_cb(void) {
    fprintf(stderr, "exit called\n");
}

static struct synergy_c_ops g_syn_ops = {
    .bgthread_cb = syn_bgthread_cb,
    .change_focus_cb = syn_change_focus_cb,
    .keypress_cb = syn_keypress_cb,
    .exit_cb = syn_exit_cb,
};

__attribute__((constructor))
static void init(void) {
    fprintf(stderr, "costructor!\n");
    
    pthread_cond_init(&g_cv, NULL);
    syn_reinit(&g_syn_ops);
}
