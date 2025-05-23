#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <termios.h>
#include <alsa/asoundlib.h>
#include <FLAC/stream_decoder.h>

// 105 lines of flac player :)

snd_pcm_t *pcm = NULL;
volatile float volume = 1.0f;

void *input_thread(void *arg){
	struct termios oldt, newt;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	int ch;
	while((ch = getchar()) != EOF){
		if(ch == '1'){
			volume -= 0.1f;
			if(volume < 0.0f) volume = 0.0f;
			printf("\rvolume: %.0f%% ", volume * 100);
			fflush(stdout);
		} else if (ch == '2'){
			volume += 0.1f;
			if(volume > 2.0f) volume = 2.0f;
			printf("\rvolume: %.0f%% ", volume * 100);
			fflush(stdout);
		}
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return NULL;
}

FLAC__StreamDecoderWriteStatus write_callback(
	const FLAC__StreamDecoder *decoder,
	const FLAC__Frame *frame,
	const FLAC__int32 * const buffer[],
	void *client_data)
{
	int channels = frame->header.channels;
	int blocksize = frame->header.blocksize;
	unsigned sample_rate = frame->header.sample_rate;

	if(!pcm){
	        snd_pcm_hw_params_t *params;
        	snd_pcm_open(&pcm, "default", SND_PCM_STREAM_PLAYBACK, 0);
        	snd_pcm_hw_params_malloc(&params);
        	snd_pcm_hw_params_any(pcm, params);
        	snd_pcm_hw_params_set_access(pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        	snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);
        	snd_pcm_hw_params_set_channels(pcm, params, channels);
        	snd_pcm_hw_params_set_rate_near(pcm, params, &sample_rate, 0);
        	snd_pcm_hw_params(pcm, params);
        	snd_pcm_hw_params_free(params);
	}

	short buffer_out[blocksize * channels];
	for(int i = 0; i < blocksize; ++i){
		for(int ch = 0; ch < channels; ++ch){
			int sample = buffer[ch][i];
			sample = (int)(sample * volume / 256.0);

			if(sample > 32767) sample = 32767;
			else if(sample < -32768) sample = -32768;

			buffer_out[i * channels + ch] = sample;
		}
	}

	int err = snd_pcm_writei(pcm, buffer_out, blocksize);
	if(err < 0) snd_pcm_prepare(pcm);

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void metadata_callback(const FLAC__StreamDecoder *d, const FLAC__StreamMetadata *m, void *c) {}
void error_callback(const FLAC__StreamDecoder *d, FLAC__StreamDecoderErrorStatus s, void *c) {}

int main(int argc, char **argv){
	if(argc < 2)return 1;

	pthread_t input;
	pthread_create(&input, NULL, input_thread, NULL);

	FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();
	FLAC__StreamDecoderInitStatus status = FLAC__stream_decoder_init_file(
		decoder, argv[1], write_callback, metadata_callback, error_callback, NULL);

	if(status != FLAC__STREAM_DECODER_INIT_STATUS_OK) return 1;

	FLAC__stream_decoder_process_until_end_of_stream(decoder);
	FLAC__stream_decoder_delete(decoder);

	if(pcm){
		snd_pcm_drain(pcm);
		snd_pcm_close(pcm);
	}

	return 0;
}
