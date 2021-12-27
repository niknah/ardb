/*
 *Copyright (c) 2013-2013, yinqiwen <yinqiwen@gmail.com>
 *All rights reserved.
 *
 *Redistribution and use in source and binary forms, with or without
 *modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of Redis nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 *THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef LMDBX_ENGINE_HPP_
#define LMDBX_ENGINE_HPP_

#include "mdbx.h"
#include "db/engine.hpp"
#include "channel/all_includes.hpp"
#include "thread/thread_local.hpp"
#include "thread/thread_mutex_lock.hpp"
#include "thread/spin_rwlock.hpp"
#include "thread/event_condition.hpp"
#include "util/concurrent_queue.hpp"
#include <stack>

namespace ardb
{
    class LMDBXEngine;
    class LMDBXIterator: public Iterator
    {
        private:
            LMDBXEngine *m_engine;
            MDBX_cursor * m_cursor;
            Data m_ns;
            KeyObject m_key;
            ValueObject m_value;
            MDBX_val m_raw_key;
            MDBX_val m_raw_val;
            KeyObject m_iterate_upper_bound_key;
            bool m_valid;

            void DoJump(const KeyObject& next);

            bool Valid();
            void Next();
            void Prev();
            void Jump(const KeyObject& next);
            void JumpToFirst();
            void JumpToLast();
            KeyObject& Key(bool clone_str);
            ValueObject& Value(bool clone_str);
            Slice RawKey();
            Slice RawValue();
            void Del();
            void SetCursor(MDBX_cursor *cursor)
            {
                m_cursor = cursor;
            }
            void ClearState();
            void CheckBound();
            friend class LMDBXEngine;
        public:
            LMDBXIterator(LMDBXEngine * e, const Data& ns) :
                    m_engine(e), m_cursor(NULL),m_ns(ns), m_valid(true)
            {
            }
            KeyObject& IterateUpperBoundKey()
            {
                return m_iterate_upper_bound_key;
            }
            void MarkValid(bool valid)
            {
                m_valid = valid;
            }
            ~LMDBXIterator();
    };

    struct LMDBXConfig
    {
            std::string path;
            int64 max_dbsize;
            int64 max_dbs;
            int64 batch_commit_watermark;
            bool readahead;
            LMDBXConfig() :
                    max_dbsize(10 * 1024 * 1024 * 1024LL), max_dbs(4096), batch_commit_watermark(1024), readahead(false)
            {
            }
    };

    class LMDBXEngine: public Engine
    {
        private:
            MDBX_env *m_env;
            MDBX_dbi m_meta_dbi;
            typedef TreeMap<Data, MDBX_dbi>::Type DBITable;
            LMDBXConfig m_cfg;
            std::string m_dbdir;

            DBITable m_dbis;
            SpinRWLock m_lock;
            ThreadMutex m_backup_lock;
            friend class LMDBXIterator;
            bool GetDBI(Context& ctx, const Data& name, bool create_if_noexist, MDBX_dbi& dbi);
            int Reopen(const LMDBXConfig& cfg);
            int Close();
        public:
            LMDBXEngine();
            ~LMDBXEngine();
            int Init(const std::string& dir, const std::string& options);
            int Repair(const std::string& dir);
            int Put(Context& ctx, const KeyObject& key, const ValueObject& value);
            int PutRaw(Context& ctx, const Data& ns, const Slice& key, const Slice& value);
            int Get(Context& ctx, const KeyObject& key, ValueObject& value);
            int MultiGet(Context& ctx, const KeyObjectArray& keys, ValueObjectArray& values, ErrCodeArray& errs);
            int Del(Context& ctx, const KeyObject& key);
            int Merge(Context& ctx, const KeyObject& key, uint16_t op, const DataArray& args);
            bool Exists(Context& ctx, const KeyObject& key,ValueObject& val);
            int BeginWriteBatch(Context& ctx);
            int CommitWriteBatch(Context& ctx);
            int DiscardWriteBatch(Context& ctx);
            int Compact(Context& ctx, const KeyObject& start, const KeyObject& end);
            int ListNameSpaces(Context& ctx, DataArray& nss);
            int DropNameSpace(Context& ctx, const Data& ns);
            void Stats(Context& ctx, std::string& str);
            int64_t EstimateKeysNum(Context& ctx, const Data& ns);
            Iterator* Find(Context& ctx, const KeyObject& key);
            const std::string GetErrorReason(int err);
            int Backup(Context& ctx, const std::string& dir);
            int Restore(Context& ctx, const std::string& dir);
            const FeatureSet GetFeatureSet()
            {
                FeatureSet features;
                features.support_compactfilter = 0;
                features.support_namespace = 1;
                features.support_merge = 0;
                features.support_backup = 1;
                return features;
            }
            int MaxOpenFiles()
            {
                return 10;
            }

    };
}

#endif /* LMDBX_ENGINE_HPP_ */
