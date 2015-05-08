/*

Copyright (c) 2008, Arvid Norberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in
      the documentation and/or other materials provided with the distribution.
    * Neither the name of the author nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#ifndef TORRENT_ERROR_CODE_HPP_INCLUDED
#define TORRENT_ERROR_CODE_HPP_INCLUDED

#include "config.hpp"

#if defined TORRENT_WINDOWS || defined TORRENT_CYGWIN
#include <winsock2.h>
#endif

namespace boost
{
namespace system
{
    class error_category //: public boost::noncopyable
    {
    public:
      virtual ~error_category(){}

	  virtual const char *     name() const {return "system error";};
	  virtual std::string      message( int ev ) const {return "unknown error";}

      bool operator==(const error_category & rhs) const { return this == &rhs; }
      bool operator!=(const error_category & rhs) const { return this != &rhs; }
      bool operator<( const error_category & rhs ) const
      {
        return std::less<const error_category*>()( this, &rhs );
      }
    };

	inline error_category& get_system_category()
	{
		static error_category system_category;
		return system_category;
	}


    class error_code
    {
    public:

      // constructors:
      error_code( int val = 0 ) : m_val(val), m_cat(&get_system_category()) {}
      error_code( int val, const error_category & cat ) : m_val(val), m_cat(&cat) {}


      void clear()
      {
        m_val = 0;
        m_cat = &get_system_category();
      }

      // observers:
      int                     value() const    { return m_val; }
      const error_category &  category() const { return *m_cat; }
      std::string             message() const  { return m_cat->message(value()); }

      typedef void (*unspecified_bool_type)();
      static void unspecified_bool_true() {}

      operator unspecified_bool_type() const  // true if error
      { 
        return m_val == 0 ? 0 : unspecified_bool_true;
      }

      bool operator!() const  // true if no error
      {
        return m_val == 0;
      }

      // relationals:
      inline friend bool operator==( const error_code & lhs,
                                     const error_code & rhs )
        //  the more symmetrical non-member syntax allows enum
        //  conversions work for both rhs and lhs.
      {
        return lhs.m_cat == rhs.m_cat && lhs.m_val == rhs.m_val;
      }

      inline friend bool operator<( const error_code & lhs,
                                    const error_code & rhs )
        //  the more symmetrical non-member syntax allows enum
        //  conversions work for both rhs and lhs.
      {
        return lhs.m_cat < rhs.m_cat
          || (lhs.m_cat == rhs.m_cat && lhs.m_val < rhs.m_val);
      }

      private:
      int                     m_val;
      const error_category *  m_cat;

    };
} }
using boost::system::error_code;
#include "string_util.hpp" // for allocate_string_copy
#include <stdlib.h> // free

namespace libtorrent
{

	namespace errors
	{
		enum error_code_enum
		{
			no_error = 0,
			file_collision,
			failed_hash_check,
			torrent_is_no_dict,
			torrent_missing_info,
			torrent_info_no_dict,
			torrent_missing_piece_length,
			torrent_missing_name,
			torrent_invalid_name,
			torrent_invalid_length,
			torrent_file_parse_failed,
			torrent_missing_pieces,
			torrent_invalid_hashes,
			too_many_pieces_in_torrent,
			invalid_swarm_metadata,
			invalid_bencoding,
			no_files_in_torrent,
			invalid_escaped_string,
			session_is_closing,
			duplicate_torrent,
			invalid_torrent_handle,
			invalid_entry_type,
			missing_info_hash_in_uri,
			file_too_short,
			unsupported_url_protocol,
			url_parse_error,
			peer_sent_empty_piece,
			parse_failed,
			invalid_file_tag,
			missing_info_hash,
			mismatching_info_hash,
			invalid_hostname,
			invalid_port,
			port_blocked,
			expected_close_bracket_in_address,
			destructing_torrent,
			timed_out,
			upload_upload_connection,
			uninteresting_upload_peer,
			invalid_info_hash,
			torrent_paused,
			invalid_have,
			invalid_bitfield_size,
			too_many_requests_when_choked,
			invalid_piece,
			no_memory,
			torrent_aborted,
			self_connection,
			invalid_piece_size,
			timed_out_no_interest,
			timed_out_inactivity,
			timed_out_no_handshake,
			timed_out_no_request,
			invalid_choke,
			invalid_unchoke,
			invalid_interested,
			invalid_not_interested,
			invalid_request,
			invalid_hash_list,
			invalid_hash_piece,
			invalid_cancel,
			invalid_dht_port,
			invalid_suggest,
			invalid_have_all,
			invalid_have_none,
			invalid_reject,
			invalid_allow_fast,
			invalid_extended,
			invalid_message,
			sync_hash_not_found,
			invalid_encryption_constant,
			no_plaintext_mode,
			no_rc4_mode,
			unsupported_encryption_mode,
			unsupported_encryption_mode_selected,
			invalid_pad_size,
			invalid_encrypt_handshake,
			no_incoming_encrypted,
			no_incoming_regular,
			duplicate_peer_id,
			torrent_removed,
			packet_too_large,
			reserved,
			http_error,
			missing_location,
			invalid_redirection,
			redirecting,
			invalid_range,
			no_content_length,
			banned_by_ip_filter,
			too_many_connections,
			peer_banned,
			stopping_torrent,
			too_many_corrupt_pieces,
			torrent_not_ready,
			peer_not_constructed,
			session_closing,
			optimistic_disconnect,
			torrent_finished,
			no_router,
			metadata_too_large,
			invalid_metadata_request,
			invalid_metadata_size,
			invalid_metadata_offset,
			invalid_metadata_message,
			pex_message_too_large,
			invalid_pex_message,
			invalid_lt_tracker_message,
			too_frequent_pex,
			no_metadata,
			invalid_dont_have,
			requires_ssl_connection,
			invalid_ssl_cert,
			reserved113,
			reserved114,
			reserved115,
			reserved116,
			reserved117,
			reserved118,
			reserved119,

// natpmp errors
			unsupported_protocol_version, // 120
			natpmp_not_authorized,
			network_failure,
			no_resources,
			unsupported_opcode,
			reserved125,
			reserved126,
			reserved127,
			reserved128,
			reserved129,

// fastresume errors
			missing_file_sizes, // 130
			no_files_in_resume_data,
			missing_pieces,
			mismatching_number_of_files,
			mismatching_file_size,
			mismatching_file_timestamp,
			not_a_dictionary,
			invalid_blocks_per_piece,
			missing_slots,
			too_many_slots,
			invalid_slot_list,
			invalid_piece_index,
			pieces_need_reorder,
			reserved143,
			reserved144,
			reserved145,
			reserved146,
			reserved147,
			reserved148,
			reserved149,

// HTTP errors
			http_parse_error, // 150
			http_missing_location,
			http_failed_decompress,
			reserved153,
			reserved154,
			reserved155,
			reserved156,
			reserved157,
			reserved158,
			reserved159,

// i2p errors
			no_i2p_router, // 160
			reserved161,
			reserved162,
			reserved163,
			reserved164,
			reserved165,
			reserved166,
			reserved167,
			reserved168,
			reserved169,

// tracker errors
			scrape_not_available, // 170
			invalid_tracker_response,
			invalid_peer_dict,
			tracker_failure,
			invalid_files_entry,
			invalid_hash_entry,
			invalid_peers_entry,
			invalid_tracker_response_length,
			invalid_tracker_transaction_id,
			invalid_tracker_action,
			reserved180,
			reserved181,
			reserved182,
			reserved183,
			reserved184,
			reserved185,
			reserved186,
			reserved187,
			reserved188,
			reserved189,

// bdecode errors
			expected_string, // 190
			expected_colon,
			unexpected_eof,
			expected_value,
			depth_exceeded,
			limit_exceeded,

			error_code_max
		};

		enum http_errors
		{
			cont = 100,
			ok = 200,
			created = 201,
			accepted = 202,
			no_content = 204,
			multiple_choices = 300,
			moved_permanently = 301,
			moved_temporarily = 302,
			not_modified = 304,
			bad_request = 400,
			unauthorized = 401,
			forbidden = 403,
			not_found = 404,
			internal_server_error = 500,
			not_implemented = 501,
			bad_gateway = 502,
			service_unavailable = 503
		};
	}
}

namespace libtorrent
{
	struct TORRENT_EXPORT libtorrent_error_category : boost::system::error_category
	{
		virtual const char* name() const;
		virtual std::string message(int ev) const;
	};

	inline boost::system::error_category& get_libtorrent_category()
	{
		static libtorrent_error_category libtorrent_category;
		return libtorrent_category;
	}

	namespace errors
	{
		inline boost::system::error_code make_error_code(error_code_enum e)
		{
			return boost::system::error_code(e, get_libtorrent_category());
		}
	}

	using boost::system::error_code;

#ifndef BOOST_NO_EXCEPTIONS
	struct TORRENT_EXPORT libtorrent_exception: std::exception
	{
		libtorrent_exception(error_code const& s): m_error(s), m_msg(0) {}
		virtual const char* what() const throw();
		virtual ~libtorrent_exception() throw();
		error_code error() const { return m_error; }
	private:
		error_code m_error;
		mutable char* m_msg;
	};
#endif
}

#endif

