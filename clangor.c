#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <math.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <fftw3.h>
#include <pthread.h>
//#include <stropts.h>
#include <poll.h>
#include "util.h"

////// Errors

/* void error(char* error, ...) { */
/*   va_list args; */
/*   va_start(args, error); */
/*   vfprintf(stderr, error, args); */
/*   exit(22); */
/* } */

////// Lists

typedef struct list_s {
  int length;
  int alloc_size;
  void** data;
} list_t;

#define DEFAULT_LIST_SIZE 8

list_t* list_new(void) {
  list_t* l = malloc(sizeof(list_t));
  if (l == NULL) {
    error("malloc error in list_new\n");
  }
  l->length = 0;
  l->alloc_size = DEFAULT_LIST_SIZE;
  l->data = malloc(DEFAULT_LIST_SIZE*sizeof(void*));
  return l;
}

void list_free(list_t* l) {
  free(l->data);
  free(l);
}

void list__maybe_realloc(list_t* list) {
  if (list->length > list->alloc_size) {
    list->alloc_size *= 2;
    list->data = realloc(list->data, list->alloc_size*sizeof(void*));
    if (list->data == NULL) {
      error("Error realloc in list__maybe_realloc\n");
    }
  }
}

void list_append(list_t* list, void* elt) {
  list->length++;
  list__maybe_realloc(list);
  list->data[list->length-1] = elt;
}

void list_insert(list_t* list, int i, void* elt) {
  list->length++;
  list__maybe_realloc(list);
  if (i < 0) {
    i += list->length;
  }
  if (i < 0 || i >= list->length) {
    error("List index %d out of bounds.\n", i);
  }
  for (int j = list->length - 1; j > i; j--) {
    list->data[j] = list->data[j-1];
  }
  list->data[i] = elt;
}

void* list_get(list_t* list, int i) {
  if (i < 0) {
    i += list->length;
  }
  if (i < 0 || i >= list->length) {
    error("List index %d out of bounds.\n", i);
  }
  return list->data[i];
}

int list_index(list_t* list, void* elt) {
  for (int i = 0; i < list->length; i++) {
    if (list_get(list, i) == elt) {
      return i;
    }
  }
  return -1;
}

bool list_in(list_t* list, void* elt) {
  return list_index(list, elt) >= 0;
}

void set_add(list_t* list, void* elt) {
  if (!list_in(list, elt)) {
    list_append(list, elt);
  }
}

bool list_remove(list_t* list, void* elt) {
  for (int i = 0; i < list->length; i++) {
    if (list_get(list, i) == elt) {
      list->length--;
      for (; i < list->length; i++) {
        list->data[i] = list->data[i+1];
      }
      return true;
    }
  }
  return false;
}

void* list_pop(list_t* list, int i) {
  if (i < 0) {
    i += list->length;
  }
  if (i < 0 || i >= list->length) {
    error("List index %d out of bounds.\n", i);
  }
  void* elt = list->data[i];
  list->length--;
  for (; i < list->length; i++) {
    list->data[i] = list->data[i+1];
  }
  return elt;
}

list_t* list_copy(list_t* list) {
  list_t* l = list_new();
  for (int i = 0; i < list->length; i++) {
    list_append(l, list_get(list, i));
  }
  return l;
}

////// HashMap

// Maps of void* -> void*

typedef struct HashMap_s {
  int num_bins;
  int size;
  struct HashMap_bin** bins;
} HashMap;

struct HashMap_bin {
  int size;
  void* slot[7];
};

HashMap hashmap_new() {
  HashMap* m = malloc(sizeof(HashMap));
  m->num_bins = 4;
  m->size = 0;
  m->bins = malloc(m->num_bins*sizeof(struct HashMap_bin*));
  for (int i = 0; i < m->num_bins; i++) {

  }
}

////// Audio windows

#define WINDOW_FRAMES 1024

struct Window_s;

typedef void (*window_updater)(struct Window_s*);

typedef struct Window_s {
  int num_frames;
  window_updater updater;
  void* data1;
  void* data2;
  void* data3;
  void* data4;
  list_t* dependencies;
  fftw_complex * frames;
} Window;

Window* window_new(int num_frames) {
  Window* window = malloc(sizeof(Window));
  if (window == NULL) {
    error("malloc error in window_new\n");
  }
  window->num_frames = num_frames;
  window->updater = NULL;
  window->dependencies = list_new();
  size_t size = sizeof(fftw_complex) * num_frames;
  window->frames = fftw_malloc(size);
  return window;
}

Window* window_update(Window * window) {
  if (window->updater != NULL) {
    window->updater(window);
  }
  return window;
}

void window_add_dep(Window * window, Window * dep) {
  set_add(window->dependencies, (void*)dep);
}

////// Window dependencies

// TODO complain about back-edges
list_t* make_window_dep_order(list_t* root_windows) {
  list_t* order = list_new();
  list_t* queue = list_new();
  for (int i = 0; i < root_windows->length; i++) {
    set_add(queue, list_get(root_windows, i));
  }
  while (queue->length > 0) {
    Window* w = list_pop(queue, -1);
    if (!list_in(order, (void*)w)) {
      list_insert(order, 0, (void*)w);
      for (int i = 0; i < w->dependencies->length; i++) {
        set_add(queue, list_get(w->dependencies, i));
      }
    }
  }
  return order;
}

////// Program description

typedef struct Program_s {
  list_t* windows;
  Window* left;
  Window* right;
  list_t* window_plan;
} Program;

Program* program_new(void) {
  Program* program = malloc(sizeof(Program));
  program->windows = list_new();
  program->left = NULL;
  program->right = NULL;
  program->window_plan = list_new();
  return program;
}

void program_update_plan(Program* program) {
  list_t* root_windows = list_new();
  if (program->left != NULL) {
    list_append(root_windows, program->left);
  }
  if (program->right != NULL) {
    list_append(root_windows, program->right);
  }
  list_free(program->window_plan);
  program->window_plan = make_window_dep_order(root_windows);
}

void program_follow_plan(Program* program) {
  for (int i = 0; i < program->window_plan->length; i++) {
    Window* w = list_get(program->window_plan, i);
    window_update(w);
  }
}

//////

fftw_complex * in;
fftw_complex * out;
fftw_plan ff_plan;

jack_port_t *output_port;
jack_port_t *input_port;
jack_port_t *midi_input_port;

typedef jack_default_audio_sample_t sample_t;
jack_nframes_t sr;

sample_t* cycle;
jack_nframes_t samincy;
long offset=0;

int tone=440;

int srate(jack_nframes_t nframes, void *arg) {
  printf("The sample rate is now %lu/sec\n", nframes);
  sr=nframes;
  return 0;
}

void jack_shutdown(void *arg) {
  exit(1);
}

struct {
  int read;
  int write;
} pipes;

Program* program;

int process_program(jack_nframes_t nframes, void *arg) {
  jack_midi_event_t in_event;
  jack_nframes_t event_index = 0;
  jack_position_t position;
  //  jack_transport_t transport;

  void* port_buf = jack_port_get_buffer(midi_input_port, nframes);
  //  transport = jack_transport_query(client, &position);
  jack_nframes_t event_count = jack_midi_get_event_count(port_buf);
  for (int i = 0; i < event_count; i++) {
    jack_midi_event_get(&in_event, port_buf, i);
    printf("Frame=%d, Event=%d, SubFrame=%d, Message=%d %d %d\n",
           position.frame, i, in_event.time,
           (long)in_event.buffer[0], (long)in_event.buffer[1], (long)in_event.buffer[2]);
  }

  static int prog_i = 1000000; // sentinel which makes program_follow_plan execute
  sample_t *out = (sample_t*) jack_port_get_buffer(output_port, nframes);
  for(jack_nframes_t i=0; i<nframes; i++, prog_i++) {
    if (prog_i >= program->left->num_frames) {
      program_follow_plan(program);
      prog_i = 0;
    }
    out[i] = creal(program->left->frames[prog_i]);
  }

  long c = (long)nframes;

  write(pipes.write, &c, sizeof(long));

  return 0;
}

void sin_update(Window* window) {
  float* t = (float*)&window->data1;
  float freq = *(float*)&((Window*)window->data2)->frames;
  float gain = *(float*)&((Window*)window->data3)->frames;
  float penlast = creal(window->frames[window->num_frames-2]);
  float last = creal(window->frames[window->num_frames-1]);
  bool isRising = last > penlast;
  float phase = 0;
  if (isRising) {
    phase = asin(last/gain) + 2*M_PI*freq/sr;
  } else {
    phase = -asin(last/gain) + 2*M_PI*freq/sr + M_PI;
  }
  *t += (float)window->num_frames/sr;
  for (int i = 0; i < window->num_frames; i++) {
    window->frames[i] = gain*sin(0*phase + freq*2*M_PI*(*t+(float)i/sr));
  }
}

void sum_update(Window* window) {
  for (int i = 0; i < window->num_frames; i++) {
    window->frames[i] = 0;
  }
  list_t* windows = window->data1;
  for (int j = 0; j < windows->length; j++) {
    Window* s = list_get(windows, j);
    for (int i = 0; i < window->num_frames; i++) {
      window->frames[i] += s->frames[i];
    }
  }
}

Window* make_sin(Window* freq, Window* gain) {
  Window* w = window_new(WINDOW_FRAMES);
  w->updater = sin_update;
  w->data1 = 0;
  w->data2 = freq;
  w->data3 = gain;
  window_add_dep(w, freq);
  window_add_dep(w, gain);
  return w;
}

Window* make_sum(list_t* windows) {
  Window* w = window_new(WINDOW_FRAMES);
  w->updater = sum_update;
  w->data1 = list_copy(windows);
  for (int i = 0; i < windows->length; i++) {
    window_add_dep(w, list_get(windows, i));
  }
  return w;
}

Window* make_const(float c) {
  Window* w = window_new(0);
  w->updater = NULL;
  *(float*)&w->frames = c;
  return w;
}

void sweep_update(Window* window) {
  *(float*)&window->frames *= 1.001;
}

Window* make_sweep(float c) {
  Window* w = make_const(c);
  w->updater = sweep_update;
  return w;
}

int main(int argc, char *argv[]) {
  jack_client_t *client;
  const char **ports;

  if (pipe((void*)&pipes)) {
    error ("Could not create pipe\n");
  }

  program = program_new();
  list_t* summands = list_new();
  for (int i = 1; i < 20; i++) {
    list_append(summands, make_sin(make_const(220*pow(2, i-1)),
                                   make_const(0.1/pow(2.2, i-1))));
  }
  for (int i = 1; i < 20; i++) {
    list_append(summands, make_sin(make_const(5.0/4*220*pow(2, i-1)),
                                   make_const(0.1/pow(2.2, i-1))));
  }
  for (int i = 1; i < 20; i++) {
    list_append(summands, make_sin(make_const(3.0/2*220*pow(2, i-1)),
                                   make_const(0.1/pow(2.2, i-1))));
  }
  program->left = make_sum(summands);
  //list_append(summands, make_sin(make_sweep(220), make_const(0.1)));

  program->right = window_new(WINDOW_FRAMES); //TODO make use of right
  program_update_plan(program);

  in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * WINDOW_FRAMES);
  out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * WINDOW_FRAMES);
  ff_plan = fftw_plan_dft_1d(WINDOW_FRAMES, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
  
  //  jack_set_error_functon(error);
  if(0 == (client = jack_client_open("clangor", JackNoStartServer, NULL))) {
    fprintf(stderr, "Cannot connect to Jack server.\n");
  }
  
  jack_set_process_callback(client, process_program, 0);

  jack_set_sample_rate_callback(client, srate, 0);

  jack_on_shutdown(client, jack_shutdown, 0);

  printf("Engine sample rate: %lu/sec\n", jack_get_sample_rate(client));
  
  sr=jack_get_sample_rate(client);

  output_port = jack_port_register(client, "output",
				   JACK_DEFAULT_AUDIO_TYPE,
				   JackPortIsOutput, 0);
  midi_input_port = jack_port_register(client, "midi_in",
                                       JACK_DEFAULT_MIDI_TYPE,
                                       JackPortIsInput, 0);

  samincy=(sr/tone);
  sample_t scale = 2*M_PI/samincy;
  cycle=(sample_t*)malloc(samincy*sizeof(sample_t));
  if(cycle==NULL) {
    fprintf(stderr, "memory allocation failed\n");
    return 1;
  }

  for(int i=0; i<samincy; i++) {
    cycle[i]=sin(i*scale)+sin(1+i*scale*2)/2;
  }

  if(jack_activate(client)) {
    fprintf(stderr, "Cannot activate client");
    return 1;
  }

  if((ports = jack_get_ports(client, NULL, NULL,
			     JackPortIsPhysical | JackPortIsInput)) == NULL) {
    fprintf(stderr, "Cannot find any physical playback ports\n");
    exit(1);
  }

  int i=0;
  while(ports[i]!=NULL) {
    if(jack_connect(client, jack_port_name(output_port), ports[i])) {
      fprintf(stderr, "Cannot connect output ports\n");
    }
    i++;
  }

  free(ports);

  struct pollfd fds[1];
  fds[0].fd = pipes.read;
  fds[0].events = POLLOUT;
  for(;;) {
    //sleep(3);
    int ret = poll(fds, 1, 3000);
    if (ret > 0) {
      if (fds[0].revents & POLLOUT) {
        long c;
        if (read(pipes.read, &c, sizeof(long)) > 0) {
          printf("Received long: %ld\n", c);
        }
      }
    }
  }
}
