#ifndef WLAN_EMU_TEST_PARAMS_H
#define WLAN_EMU_TEST_PARAMS_H
#include <cstring>
#include <cstdio>
#include "wlan_emu_common.h"
#include "wlan_emu_ui_mgr.h"
#include "wlan_emu_msg.h"
#include "cci_wifi_utils.hpp"

class wlan_emu_msg_mgr_t;
class wlan_emu_ui_mgr_t;
class wlan_emu_sta_mgr_t;
class wlan_emu_msg_t;


class test_step_params_t {
  public:
      wlan_emu_msg_mgr_t  *m_msg_mgr;
      wlan_emu_ui_mgr_t   *m_ui_mgr;
      wlan_emu_sta_mgr_t  *m_sta_mgr;
      wlan_emu_test_case_config *m_step_parent_test_config;
      step_param_type_t param_type;
      /* Common Start*/
      char test_case_name[128];
      char test_case_id[8];
      unsigned int step_number; //User input
      unsigned int step_seq_num; //step Sequence number

      bool capture_frames; //TestCapture
      queue_t   *test_results_queue; //is output wlan_emu_pcap_captures
      queue_t   *test_reference_queue; //is of type  wlan_emu_pcap_captures
      frame_capture_request_t frame_request;
      wlan_emu_tests_state_t test_state;
      bool fork; //true will be running in parallel else in serial
      pthread_mutex_t s_lock;//if running in parallel
      int execution_time;
      int timeout_count;

      /* Common End*/
      union {
          //Based on type : wlan_emu_test_param_type.
          //test_json will be applicable for  test_param_type_radio, test_param_type_vap,
          char test_webconfig_json[128]; //location

          sta_test_t sta_test;

          timed_wait_t *timed_wait;
          log_redirect_t *log_capture;
          command *cmd;
          wifi_stats_get_t *wifi_stats_get;
      } u;
      virtual int step_execute() = 0;
      virtual int step_timeout() = 0;
      virtual int step_upload_files(FILE *output_file, bool *update_to_tda) = 0;
      virtual void step_remove() = 0;
      virtual int step_frame_filter(wlan_emu_msg_t *msg) = 0;
      int rbus_send(char *data);
      inline void param_add_msg_mgr(wlan_emu_msg_mgr_t *mgr) { m_msg_mgr = mgr; }
      inline void param_add_ui_mgr(wlan_emu_ui_mgr_t *mgr) { m_ui_mgr = mgr; }
      inline void param_add_sta_mgr(wlan_emu_sta_mgr_t *mgr) { m_sta_mgr = mgr; }
      inline void param_add_test_config(wlan_emu_test_case_config * mgr) { m_step_parent_test_config = mgr; }
      inline wlan_emu_sta_mgr_t *param_get_sta_mgr() { return m_sta_mgr; }
      inline wlan_emu_test_case_config *param_get_test_case_config() { return  m_step_parent_test_config;}
      inline void step_add_mgr_data(wlan_emu_msg_mgr_t *msg_mgr, wlan_emu_ui_mgr_t *ui_mgr, wlan_emu_sta_mgr_t *sta_mgr, wlan_emu_test_case_config *test_config)
      {
          param_add_msg_mgr(msg_mgr);
          param_add_ui_mgr(ui_mgr);
          param_add_sta_mgr(sta_mgr);
          param_add_test_config(test_config);
      }
};

class test_step_param_vap : public test_step_params_t  {
  public:
      int step_execute();
      int step_timeout();
      int step_upload_files(FILE *output_file, bool *update_to_tda);
      void step_remove();
      int step_frame_filter(wlan_emu_msg_t *msg);
      test_step_param_vap();
      ~test_step_param_vap();
};

class test_step_param_radio : public test_step_params_t  {
  public:
      int step_execute();
      int step_timeout();
      int step_upload_files(FILE *output_file, bool *update_to_tda);
      void step_remove();
      int step_frame_filter(wlan_emu_msg_t *msg);
      test_step_param_radio();
      ~test_step_param_radio();
};


class test_step_param_sta : public test_step_params_t  {
  public:
      virtual int step_execute() = 0;
      virtual int step_timeout() = 0;
      virtual int step_upload_files(FILE *output_file, bool *update_to_tda) = 0;
      virtual int step_frame_filter(wlan_emu_msg_t *msg) = 0;
      virtual void step_remove() = 0;
};

class test_step_param_sta_management : public test_step_param_sta {
  public:
      int decode_user_ap_config(cJSON *sta_root_json, wifi_vap_info_t *ap_vap_info);
      int update_sta_config(wifi_vap_info_t *ap_vap_config);
      int decode_step_sta_management_config();
      int step_execute();
      int step_timeout();
      int step_upload_files(FILE *output_file, bool *update_to_tda);
      void step_remove();
      int step_frame_filter(wlan_emu_msg_t *msg);
      test_step_param_sta_management();
      ~test_step_param_sta_management();
};


class test_step_param_command : public test_step_params_t  {
  public:
      int step_execute();
      int step_timeout();
      int step_upload_files(FILE *output_file, bool *update_to_tda);
      void step_remove();
      int step_frame_filter(wlan_emu_msg_t *msg);
      test_step_param_command();
      ~test_step_param_command();
};


class test_step_param_dmlsubdoc : public test_step_params_t  {
  public:
      int step_execute();
      int step_timeout();
      int step_upload_files(FILE* output_file, bool *update_to_tda);
      void step_remove();
      int step_frame_filter(wlan_emu_msg_t *msg);
      test_step_param_dmlsubdoc();
      ~test_step_param_dmlsubdoc();
};

class test_step_param_logredirect : public test_step_params_t  {
  public:
      int step_execute();
      int step_timeout();
      int step_upload_files(FILE* output_file, bool *update_to_tda);
      void step_remove();
      int step_frame_filter(wlan_emu_msg_t *msg);
      test_step_param_logredirect();
      ~test_step_param_logredirect();
      static void *log_thread_function(void *arg);
};

class test_step_param_dml_reset : public test_step_params_t  {
  public:
      int step_execute();
      int step_timeout();
      int step_upload_files(FILE* output_file, bool *update_to_tda);
      void step_remove();
      int step_frame_filter(wlan_emu_msg_t *msg);
      test_step_param_dml_reset();
      ~test_step_param_dml_reset();
};

class test_step_param_get_stats_t : public test_step_params_t  {
  public:
      int step_execute();
      int step_timeout();
      int step_upload_files(FILE* output_file, bool *update_to_tda);
      void step_remove();
      int start_subscription();
      int stop_subscription(test_step_params_t *step);
      char* getStatsClass();
      char* get_scanmode();
      int get_subscription_string(char *str, int str_len);
      static void stats_get_event_handler(rbusHandle_t handle, rbusEvent_t const* event, rbusEventSubscription_t* subscription);
      int step_frame_filter(wlan_emu_msg_t *msg);
};
class test_step_param_get_radio_channel_stats : public test_step_param_get_stats_t {
  public:
      test_step_param_get_radio_channel_stats();
      ~test_step_param_get_radio_channel_stats();
};
class test_step_param_get_neighbor_stats : public test_step_param_get_stats_t  {
  public:
      test_step_param_get_neighbor_stats();
      ~test_step_param_get_neighbor_stats();
};
class test_step_param_get_assoc_clients_stats : public test_step_param_get_stats_t  {
  public:
      test_step_param_get_assoc_clients_stats();
      ~test_step_param_get_assoc_clients_stats();
};
class test_step_param_get_radio_diag_stats : public test_step_param_get_stats_t  {
  public:
      test_step_param_get_radio_diag_stats();
      ~test_step_param_get_radio_diag_stats();
};
class test_step_param_get_radio_temperature_stats : public test_step_param_get_stats_t  {
  public:
      test_step_param_get_radio_temperature_stats();
      ~test_step_param_get_radio_temperature_stats();
};
#endif
