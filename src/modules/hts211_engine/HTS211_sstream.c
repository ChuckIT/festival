/* ----------------------------------------------------------------- */
/*           The HMM-Based Speech Synthesis System (HTS)             */
/*           hts_engine API developed by HTS Working Group           */
/*           http://hts-engine.sourceforge.net/                      */
/* ----------------------------------------------------------------- */
/*                                                                   */
/*  Copyright (c) 2001-2010  Nagoya Institute of Technology          */
/*                           Department of Computer Science          */
/*                                                                   */
/*                2001-2008  Tokyo Institute of Technology           */
/*                           Interdisciplinary Graduate School of    */
/*                           Science and Engineering                 */
/*                                                                   */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/* - Redistributions of source code must retain the above copyright  */
/*   notice, this list of conditions and the following disclaimer.   */
/* - Redistributions in binary form must reproduce the above         */
/*   copyright notice, this list of conditions and the following     */
/*   disclaimer in the documentation and/or other materials provided */
/*   with the distribution.                                          */
/* - Neither the name of the HTS working group nor the names of its  */
/*   contributors may be used to endorse or promote products derived */
/*   from this software without specific prior written permission.   */
/*                                                                   */
/* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND            */
/* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,       */
/* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF          */
/* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE          */
/* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS */
/* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,          */
/* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED   */
/* TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,     */
/* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON */
/* ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,   */
/* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY    */
/* OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE           */
/* POSSIBILITY OF SUCH DAMAGE.                                       */
/* ----------------------------------------------------------------- */

#ifndef HTS211_SSTREAM_C
#define HTS211_SSTREAM_C

#ifdef __cplusplus
#define HTS211_SSTREAM_C_START extern "C" {
#define HTS211_SSTREAM_C_END   }
#else
#define HTS211_SSTREAM_C_START
#define HTS211_SSTREAM_C_END
#endif                          /* __CPLUSPLUS */

HTS211_SSTREAM_C_START;

/* hts_engine libraries */
#include "HTS211_hidden.h"

static void HTS211_set_duration(int *duration, double *mean, double *vari,
                             double *remain, int size, double frame_length)
{
   int i;
   double temp1, temp2;
   double rho = 0.0;

   if (frame_length != 0.0) {   /* if frame length is specified, rho is determined */
      temp1 = 0.0;
      temp2 = 0.0;
      for (i = 0; i < size; i++) {
         temp1 += mean[i];
         temp2 += vari[i];
      }
      rho = (frame_length - temp1) / temp2;
   }
   for (i = 0; i < size; i++) {
      temp1 = mean[i] + rho * vari[i] + *remain;
      duration[i] = (int) (temp1 + 0.5);
      if (duration[i] < 1)
         duration[i] = 1;
      *remain = temp1 - (double) duration[i];
   }
}

/* HTS211_SStreamSet_initialize: initialize state stream set */
void HTS211_SStreamSet_initialize(HTS211_SStreamSet * sss)
{
   sss->nstream = 0;
   sss->nstate = 0;
   sss->sstream = NULL;
   sss->duration = NULL;
   sss->total_state = 0;
   sss->total_frame = 0;
}

/* HTS211_SStreamSet_create: parse label and determine state duration */
void HTS211_SStreamSet_create(HTS211_SStreamSet * sss, HTS211_ModelSet * ms,
                           HTS211_Label * label, double *duration_iw,
                           double **parameter_iw, double **gv_iw)
{
   int i, j, k;
   double temp1, temp2;
   int state;
   HTS211_SStream *sst;
   double *duration_mean, *duration_vari;
   double duration_remain;
   double frame_length;
   int next_time;
   int next_state;

   /* initialize state sequence */
   sss->nstate = HTS211_ModelSet_get_nstate(ms);
   sss->nstream = HTS211_ModelSet_get_nstream(ms);
   sss->total_frame = 0;
   sss->total_state = HTS211_Label_get_size(label) * sss->nstate;
   sss->duration = (int *) HTS211_calloc(sss->total_state, sizeof(int));
   sss->sstream = (HTS211_SStream *) HTS211_calloc(sss->nstream, sizeof(HTS211_SStream));
   for (i = 0; i < sss->nstream; i++) {
      sst = &sss->sstream[i];
      sst->vector_length = HTS211_ModelSet_get_vector_length(ms, i);
      sst->mean = (double **) HTS211_calloc(sss->total_state, sizeof(double *));
      sst->vari = (double **) HTS211_calloc(sss->total_state, sizeof(double *));
      if (HTS211_ModelSet_is_msd(ms, i))
         sst->msd = (double *) HTS211_calloc(sss->total_state, sizeof(double));
      else
         sst->msd = NULL;
      for (j = 0; j < sss->total_state; j++) {
         sst->mean[j] =
             (double *) HTS211_calloc(sst->vector_length, sizeof(double));
         sst->vari[j] =
             (double *) HTS211_calloc(sst->vector_length, sizeof(double));
      }
      sst->gv_switch =
          (HTS211_Boolean *) HTS211_calloc(sss->total_state, sizeof(HTS211_Boolean));
      for (j = 0; j < sss->total_state; j++)
         sst->gv_switch[j] = TRUE;
   }

   /* check interpolation weights */
   for (i = 0, temp1 = 0.0;
        i < HTS211_ModelSet_get_duration_interpolation_size(ms); i++)
      temp1 += duration_iw[i];
   for (i = 0; i < HTS211_ModelSet_get_duration_interpolation_size(ms); i++)
      if (duration_iw[i] != 0.0)
         duration_iw[i] /= temp1;
   for (i = 0; i < sss->nstream; i++) {
      for (j = 0, temp1 = 0.0;
           j < HTS211_ModelSet_get_parameter_interpolation_size(ms, i); j++)
         temp1 += parameter_iw[i][j];
      for (j = 0; j < HTS211_ModelSet_get_parameter_interpolation_size(ms, i); j++)
         if (parameter_iw[i][j] != 0.0)
            parameter_iw[i][j] /= temp1;
      if (HTS211_ModelSet_use_gv(ms, i)) {
         for (j = 0, temp1 = 0.0;
              j < HTS211_ModelSet_get_gv_interpolation_size(ms, i); j++)
            temp1 += gv_iw[i][j];
         for (j = 0; j < HTS211_ModelSet_get_gv_interpolation_size(ms, i); j++)
            if (gv_iw[i][j] != 0.0)
               gv_iw[i][j] /= temp1;
      }
   }

   /* determine state duration */
   duration_mean =
       (double *) HTS211_calloc(sss->nstate * HTS211_Label_get_size(label),
                             sizeof(double));
   duration_vari =
       (double *) HTS211_calloc(sss->nstate * HTS211_Label_get_size(label),
                             sizeof(double));
   duration_remain = 0.0;
   for (i = 0; i < HTS211_Label_get_size(label); i++)
      HTS211_ModelSet_get_duration(ms, HTS211_Label_get_string(label, i),
                                &duration_mean[i * sss->nstate],
                                &duration_vari[i * sss->nstate], duration_iw);
   if (HTS211_Label_get_frame_specified_flag(label)) {
      /* use duration set by user */
      next_time = 0;
      next_state = 0;
      state = 0;
      for (i = 0; i < HTS211_Label_get_size(label); i++) {
         temp1 = HTS211_Label_get_start_frame(label, i);
         temp2 = HTS211_Label_get_end_frame(label, i);
         if (temp2 >= 0) {
            HTS211_set_duration(&sss->duration[next_state],
                             &duration_mean[next_state],
                             &duration_vari[next_state], &duration_remain,
                             state + sss->nstate - next_state,
                             temp2 - next_time);
            for (j = next_state; j < state + sss->nstate; j++)
               next_time += sss->duration[j];
            next_state = state + sss->nstate;
         } else if (i + 1 == HTS211_Label_get_size(label)) {
            HTS211_set_duration(&sss->duration[next_state],
                             &duration_mean[next_state],
                             &duration_vari[next_state], &duration_remain,
                             state + sss->nstate - next_state, 0.0);
         }
         state += sss->nstate;
      }
   } else {
      /* determine frame length */
      if (HTS211_Label_get_speech_speed(label) != 1.0) {
         temp1 = 0.0;
         for (i = 0; i < HTS211_Label_get_size(label) * sss->nstate; i++) {
            temp1 += duration_mean[i];
         }
         frame_length = temp1 / HTS211_Label_get_speech_speed(label);
      } else {
         frame_length = 0.0;
      }
      /* set state duration */
      HTS211_set_duration(sss->duration, duration_mean, duration_vari,
                       &duration_remain,
                       HTS211_Label_get_size(label) * sss->nstate, frame_length);
   }
   HTS211_free(duration_mean);
   HTS211_free(duration_vari);

   /* get parameter */
   for (i = 0, state = 0; i < HTS211_Label_get_size(label); i++) {
      for (j = 2; j <= sss->nstate + 1; j++) {
         sss->total_frame += sss->duration[state];
         for (k = 0; k < sss->nstream; k++) {
            sst = &sss->sstream[k];
            if (sst->msd)
               HTS211_ModelSet_get_parameter(ms, HTS211_Label_get_string(label, i),
                                          sst->mean[state], sst->vari[state],
                                          &sst->msd[state], k, j,
                                          parameter_iw[k]);
            else
               HTS211_ModelSet_get_parameter(ms, HTS211_Label_get_string(label, i),
                                          sst->mean[state], sst->vari[state],
                                          NULL, k, j, parameter_iw[k]);
         }
         state++;
      }
   }

   /* copy dynamic window */
   for (i = 0; i < sss->nstream; i++) {
      sst = &sss->sstream[i];
      sst->win_size = HTS211_ModelSet_get_window_size(ms, i);
      sst->win_max_width = HTS211_ModelSet_get_window_max_width(ms, i);
      sst->win_l_width = (int *) HTS211_calloc(sst->win_size, sizeof(int));
      sst->win_r_width = (int *) HTS211_calloc(sst->win_size, sizeof(int));
      sst->win_coefficient =
          (double **) HTS211_calloc(sst->win_size, sizeof(double));
      for (j = 0; j < sst->win_size; j++) {
         sst->win_l_width[j] = HTS211_ModelSet_get_window_left_width(ms, i, j);
         sst->win_r_width[j] = HTS211_ModelSet_get_window_right_width(ms, i, j);
         if (sst->win_l_width[j] + sst->win_r_width[j] == 0)
            sst->win_coefficient[j] =
                (double *) HTS211_calloc(-2 * sst->win_l_width[j] + 1,
                                      sizeof(double));
         else
            sst->win_coefficient[j] =
                (double *) HTS211_calloc(-2 * sst->win_l_width[j], sizeof(double));
         sst->win_coefficient[j] -= sst->win_l_width[j];
         for (k = sst->win_l_width[j]; k <= sst->win_r_width[j]; k++)
            sst->win_coefficient[j][k] =
                HTS211_ModelSet_get_window_coefficient(ms, i, j, k);
      }
   }

   /* determine GV */
   for (i = 0; i < sss->nstream; i++) {
      sst = &sss->sstream[i];
      if (HTS211_ModelSet_use_gv(ms, i)) {
         sst->gv_mean =
             (double *) HTS211_calloc(sst->vector_length / sst->win_size,
                                   sizeof(double));
         sst->gv_vari =
             (double *) HTS211_calloc(sst->vector_length / sst->win_size,
                                   sizeof(double));
         HTS211_ModelSet_get_gv(ms, HTS211_Label_get_string(label, 0), sst->gv_mean,
                             sst->gv_vari, i, gv_iw[i]);
      } else {
         sst->gv_mean = NULL;
         sst->gv_vari = NULL;
      }
   }

   if (HTS211_ModelSet_have_gv_switch(ms) == TRUE)
      for (i = 0; i < HTS211_Label_get_size(label); i++)
         if (HTS211_ModelSet_get_gv_switch(ms, HTS211_Label_get_string(label, i)) ==
             FALSE)
            for (j = 0; j < sss->nstream; j++)
               for (k = 0; k < sss->nstate; k++)
                  sss->sstream[j].gv_switch[i * sss->nstate + k] = FALSE;
}

/* HTS211_SStreamSet_get_nstream: get number of stream */
int HTS211_SStreamSet_get_nstream(HTS211_SStreamSet * sss)
{
   return sss->nstream;
}

/* HTS211_SStreamSet_get_vector_length: get vector length */
int HTS211_SStreamSet_get_vector_length(HTS211_SStreamSet * sss, int stream_index)
{
   return sss->sstream[stream_index].vector_length;
}

/* HTS211_SStreamSet_is_msd: get MSD flag */
HTS211_Boolean HTS211_SStreamSet_is_msd(HTS211_SStreamSet * sss, int stream_index)
{
   return sss->sstream[stream_index].msd ? TRUE : FALSE;
}

/* HTS211_SStreamSet_get_total_state: get total number of state */
int HTS211_SStreamSet_get_total_state(HTS211_SStreamSet * sss)
{
   return sss->total_state;
}

/* HTS211_SStreamSet_get_total_frame: get total number of frame */
int HTS211_SStreamSet_get_total_frame(HTS211_SStreamSet * sss)
{
   return sss->total_frame;
}

/* HTS211_SStreamSet_get_msd: get MSD parameter */
double HTS211_SStreamSet_get_msd(HTS211_SStreamSet * sss, int stream_index,
                              int state_index)
{
   return sss->sstream[stream_index].msd[state_index];
}

/* HTS211_SStreamSet_window_size: get dynamic window size */
int HTS211_SStreamSet_get_window_size(HTS211_SStreamSet * sss, int stream_index)
{
   return sss->sstream[stream_index].win_size;
}

/* HTS211_SStreamSet_get_window_left_width: get left width of dynamic window */
int HTS211_SStreamSet_get_window_left_width(HTS211_SStreamSet * sss,
                                         int stream_index, int window_index)
{
   return sss->sstream[stream_index].win_l_width[window_index];
}

/* HTS211_SStreamSet_get_winodow_right_width: get right width of dynamic window */
int HTS211_SStreamSet_get_window_right_width(HTS211_SStreamSet * sss,
                                          int stream_index, int window_index)
{
   return sss->sstream[stream_index].win_r_width[window_index];
}

/* HTS211_SStreamSet_get_window_coefficient: get coefficient of dynamic window */
double HTS211_SStreamSet_get_window_coefficient(HTS211_SStreamSet * sss,
                                             int stream_index,
                                             int window_index,
                                             int coefficient_index)
{
   return sss->sstream[stream_index].
       win_coefficient[window_index][coefficient_index];
}

/* HTS211_SStreamSet_get_window_max_width: get max width of dynamic window */
int HTS211_SStreamSet_get_window_max_width(HTS211_SStreamSet * sss, int stream_index)
{
   return sss->sstream[stream_index].win_max_width;
}

/* HTS211_SStreamSet_use_gv: get GV flag */
HTS211_Boolean HTS211_SStreamSet_use_gv(HTS211_SStreamSet * sss, int stream_index)
{
   return sss->sstream[stream_index].gv_mean ? TRUE : FALSE;
}

/* HTS211_SStreamSet_get_duration: get state duration */
int HTS211_SStreamSet_get_duration(HTS211_SStreamSet * sss, int state_index)
{
   return sss->duration[state_index];
}

/* HTS211_SStreamSet_get_mean: get mean parameter */
double HTS211_SStreamSet_get_mean(HTS211_SStreamSet * sss,
                               int stream_index,
                               int state_index, int vector_index)
{
   return sss->sstream[stream_index].mean[state_index][vector_index];
}

/* HTS211_SStreamSet_set_mean: set mean parameter */
void HTS211_SStreamSet_set_mean(HTS211_SStreamSet * sss, int stream_index,
                             int state_index, int vector_index, double f)
{
   sss->sstream[stream_index].mean[state_index][vector_index] = f;
}

/* HTS211_SStreamSet_get_vari: get variance parameter */
double HTS211_SStreamSet_get_vari(HTS211_SStreamSet * sss,
                               int stream_index,
                               int state_index, int vector_index)
{
   return sss->sstream[stream_index].vari[state_index][vector_index];
}

/* HTS211_SStreamSet_set_vari: set variance parameter */
void HTS211_SStreamSet_set_vari(HTS211_SStreamSet * sss, int stream_index,
                             int state_index, int vector_index, double f)
{
   sss->sstream[stream_index].vari[state_index][vector_index] = f;
}

/* HTS211_SStreamSet_get_gv_mean: get GV mean parameter */
double HTS211_SStreamSet_get_gv_mean(HTS211_SStreamSet * sss,
                                  int stream_index, int vector_index)
{
   return sss->sstream[stream_index].gv_mean[vector_index];
}

/* HTS211_SStreamSet_get_gv_mean: get GV variance parameter */
double HTS211_SStreamSet_get_gv_vari(HTS211_SStreamSet * sss,
                                  int stream_index, int vector_index)
{
   return sss->sstream[stream_index].gv_vari[vector_index];
}

/* HTS211_SStreamSet_set_gv_switch: set GV switch */
void HTS211_SStreamSet_set_gv_switch(HTS211_SStreamSet * sss, int stream_index,
                                  int state_index, HTS211_Boolean i)
{
   sss->sstream[stream_index].gv_switch[state_index] = i;
}

/* HTS211_SStreamSet_get_gv_switch: get GV switch */
HTS211_Boolean HTS211_SStreamSet_get_gv_switch(HTS211_SStreamSet * sss, int stream_index,
                                         int state_index)
{
   return sss->sstream[stream_index].gv_switch[state_index];
}

/* HTS211_SStreamSet_clear: free state stream set */
void HTS211_SStreamSet_clear(HTS211_SStreamSet * sss)
{
   int i, j;
   HTS211_SStream *sst;

   if (sss->sstream) {
      for (i = 0; i < sss->nstream; i++) {
         sst = &sss->sstream[i];
         for (j = 0; j < sss->total_state; j++) {
            HTS211_free(sst->mean[j]);
            HTS211_free(sst->vari[j]);
         }
         if (sst->msd)
            HTS211_free(sst->msd);
         HTS211_free(sst->mean);
         HTS211_free(sst->vari);
         for (j = sst->win_size - 1; j >= 0; j--) {
            sst->win_coefficient[j] += sst->win_l_width[j];
            HTS211_free(sst->win_coefficient[j]);
         }
         HTS211_free(sst->win_coefficient);
         HTS211_free(sst->win_l_width);
         HTS211_free(sst->win_r_width);
         if (sst->gv_mean)
            HTS211_free(sst->gv_mean);
         if (sst->gv_vari)
            HTS211_free(sst->gv_vari);
         HTS211_free(sst->gv_switch);
      }
      HTS211_free(sss->sstream);
   }
   if (sss->duration)
      HTS211_free(sss->duration);

   HTS211_SStreamSet_initialize(sss);
}

HTS211_SSTREAM_C_END;

#endif                          /* !HTS211_SSTREAM_C */