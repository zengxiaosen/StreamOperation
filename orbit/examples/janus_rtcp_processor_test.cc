/*
 * Copyright 2016 All Rights Reserved.
 * Author: chenteng@orangelab.cn (Chen teng)
 *
 * rtp_packet_queue_test.cc
 */

#include "gtest/gtest.h"
#include "glog/logging.h"
#include <iostream>
#include "stream_service/orbit/rtp/janus_rtcp_processor.h"
using namespace std;

namespace orbit {
namespace {

class JanusRtcpProcessorTest: public testing::Test {
protected:
	virtual void SetUp() override {
	}

	virtual void TearDown() override {
	}
};

/**
 * test 5 cases about sequence number of nacks, contain:
 * 1. Ascending the list
 * 2. Descending the list
 * 3. List of disorder
 * 4. Numbers differ by more than 16
 * 5. Duplication numbers in list
 */
TEST_F(JanusRtcpProcessorTest, KeepTrackSequenceNumberCorrect) {
	//orbit::JanusRtpSeqTracker tracker;
	int case_count = 5;
	int input_count[5] = {4, 4, 4, 4, 4};
	uint16_t input_seqs[5][4] = {
		{	20000, 20001, 20002, 20003},
		{	20003, 20002, 20001, 20000},
		{	20003, 20001, 20002, 20000},
		{	20000, 20031, 20032, 20033},
		{	20000, 20001, 20001, 20002},
	};
	uint16_t expect_seqs[5][4] = {
		{	20000, 20001, 20002, 20003},
		{	20000, 20001, 20002, 20003},
		{	20000, 20001, 20002, 20003},
		{	20000, 20031, 20032, 20033},
		{	20000, 20001, 20002, 0},
	};
	int expect_count[5] = {4, 4, 4, 4, 3};
	for (int k = 0; k < case_count; k++)
	{
		//LOG(INFO) << "Case " << k+1 << " of KeepTrackSequenceNumberCorrect";
		// prepare input GSlist
		GSList *nacks = NULL, *result_nacks = NULL;
		int result_nacks_count = 0;
		char nackbuf[120];
		int res = 0;
		for (int i = 0; i < input_count[k]; i++) {
			nacks = g_slist_append(nacks, GUINT_TO_POINTER(input_seqs[k][i]));
		}

		// make a packet buffer of nacks
		res = orbit::janus_rtcp_nacks(nackbuf, 120, nacks);

		// get nacks from packet buffer
		result_nacks = janus_rtcp_get_nacks(nackbuf, res);
		result_nacks_count = g_slist_length(result_nacks);
		// test
		EXPECT_EQ(expect_count[k], result_nacks_count);
		GSList *cur_list = result_nacks;
		for (int i = 0; i < result_nacks_count; i++) {
			unsigned int seqn = GPOINTER_TO_UINT(cur_list->data);
			EXPECT_EQ(expect_seqs[k][i], seqn);

			cur_list = cur_list->next;
		}
	}
} // TEST KeepTrackSequenceNumberCorrect

} // anonymous
}  // namespace orbit
