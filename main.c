/**
*   PushToJack loopt geräusche durch wenn man die Rechte STRG-Taste Drückt.
*
*   released under WTFPL
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/input.h>

#include <jack/jack.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>

#define RELEASED 0
#define PRESSED 1

const char *dev = "/dev/input/by-path/platform-i8042-serio-0-event-kbd";

struct input_event ev;
int fd;
bool send = false;
jack_port_t *input_port;
jack_port_t *output_port;
jack_client_t *client;
ssize_t n;

int process (jack_nframes_t nframes, void *arg) {
    jack_default_audio_sample_t *in, *out;

    if (send)
        in = jack_port_get_buffer (input_port, nframes);
    else
        in = malloc(sizeof (jack_default_audio_sample_t) * nframes);
    out = jack_port_get_buffer (output_port, nframes);
    memcpy (out, in, sizeof (jack_default_audio_sample_t) * nframes);

    return 0;
}

void
jack_shutdown (void *arg)
{
    fprintf(stderr, "Jack is shutting down!\n");
    exit (1);
}

int main (int argc, char *argv[]) {
    const char **ports, **p, **sports, **sp;
    const char *client_name = "PushToJack";
    const char *server_name = NULL;
    char *OtherName;
    unsigned i,o;
    jack_options_t options = JackNullOption;
    jack_status_t status;

    if (argc < 2){
        fprintf (stderr, "\nUse PatchToJack [name]\n\nThat will conect the bridge betwean System and [name]\n\nexp: PushToJack TeamSpeak3\n\n");
        exit(1);
    }

    /* open a client connection to the JACK server */

    client = jack_client_open (client_name, options, &status, server_name);
    if (client == NULL) {
        fprintf (stderr, "jack_client_open() failed, "
                "status = 0x%2.0x\n", status);
        if (status & JackServerFailed) {
            fprintf (stderr, "Unable to connect to JACK server\n");
        }
        exit (1);
    }
    if (status & JackServerStarted) {
        fprintf (stderr, "JACK server started\n");
    }
    if (status & JackNameNotUnique) {
        client_name = jack_get_client_name(client);
        fprintf (stderr, "unique name `%s' assigned\n", client_name);
    }

    jack_set_process_callback (client, process, 0);
    jack_on_shutdown (client, jack_shutdown, 0);

    /* create two ports */

    input_port = jack_port_register (client, "input", JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
    output_port = jack_port_register (client, "output", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

    if ((input_port == NULL) || (output_port == NULL)) {
        fprintf(stderr, "no more JACK ports available\n");
        exit (1);
    }

    if (jack_activate (client)) {
        fprintf (stderr, "cannot activate client");
        exit (1);
    }

    /* connect to hardware */
    sports = jack_get_ports (client, NULL, NULL, JackPortIsPhysical|JackPortIsOutput);
    if (sports == NULL) {
        fprintf(stderr, "no physical capture ports\n");
        exit (1);
    }
    i = 0;
    p = sports;
    while (!(!p || !*p)){
        if (jack_connect(client, sports[i], jack_port_name(input_port))) {
            fprintf(stderr, "cannot connect input ports\n");
        }
        i++;
        p++;
    }

    /* connect to software */
    OtherName = malloc(strlen(argv[1])+3);
    sprintf(OtherName,"%s:*",argv[1]);
    ports = jack_get_ports (client, OtherName, NULL, JackPortIsInput);
    if (ports == NULL) {
        fprintf(stderr, "nichts gefunden unter '%s'\n",OtherName);
        exit (1);
    }
    i = 0;
    p = ports;
    while (!(!p || !*p)){
        sp = sports;
        o = 0;
        while(!(!sp || !*sp)) {
            jack_disconnect(client, sports[o], ports[i]);
            o++;
            sp++;
        }
        if (jack_connect (client, jack_port_name (output_port), ports[i])) {
            fprintf (stderr, "cannot connect output ports\n");
        }
        i++;
        p++;
    }

    free (sports);
    free (ports);

    fd = open(dev, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Cannot open %s: %s.\n", dev, strerror(errno));
        fprintf(stderr, "Ich brauche leserechte für %s.\n", dev);
        fprintf(stderr, "prbier es mal mit 'sudo chmod +r  %s'\n\n",dev);
        exit(1);
    }

    while (1) {
        n = read(fd, &ev, sizeof ev);
        if (n == (ssize_t) -1) {
            if (errno == EINTR)
                continue;
            else
                break;
        } else if (n != sizeof ev) {
            errno = EIO;
            break;
        }
        if (ev.type == EV_KEY && ev.value >= 0 && ev.value <= 2 && ((int)ev.code) == 97) {

            if (ev.value == PRESSED ) {
                send = true;
            }

            if (ev.value == RELEASED ) {
                send = false;
            }
        }
    }
    fflush(stdout);
    fprintf(stderr, "%s.\n", strerror(errno));

    jack_client_close (client);
    exit (0);
}