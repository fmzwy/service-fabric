// ------------------------------------------------------------
// Copyright (c) Microsoft Corporation.  All rights reserved.
// Licensed under the MIT License (MIT). See License.txt in the repo root for license information.
// ------------------------------------------------------------

#include "stdafx.h"

#include <boost/test/unit_test.hpp>
#include "Common/boost-taef.h"
#include "TStoreTestBase.h"

#define ALLOC_TAG 'tsTP'

namespace TStoreTests
{
   using namespace ktl;

   class StoreTestBuffer3Replica : public TStoreTestBase<KBuffer::SPtr, KBuffer::SPtr, KBufferComparer, KBufferSerializer, KBufferSerializer>
   {
   public:
      StoreTestBuffer3Replica()
      {
         Setup(3);
      }

      ~StoreTestBuffer3Replica()
      {
         Cleanup();
      }

      KBuffer::SPtr ToBuffer(__in ULONG num)
      {
          KBuffer::SPtr bufferSptr;
          auto status = KBuffer::Create(sizeof(ULONG), bufferSptr, GetAllocator());
          CODING_ERROR_ASSERT(NT_SUCCESS(status));

          auto buffer = (ULONG *)bufferSptr->GetBuffer();
          buffer[0] = num;
          return bufferSptr;
      }

      static bool EqualityFunction(KBuffer::SPtr & one, KBuffer::SPtr & two)
      {
          return *one == *two;
      }

      Common::CommonConfig config; // load the config object as its needed for the tracing to work
   };

   BOOST_FIXTURE_TEST_SUITE(StoreTestBuffer3ReplicaSuite, StoreTestBuffer3Replica)

   BOOST_AUTO_TEST_CASE(Add_SingleTransaction_ShouldSucceed)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, Common::TimeSpan::MaxValue, CancellationToken::None));
         SyncAwait(VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
         SyncAwait(tx->CommitAsync());
      }

      SyncAwait(VerifyKeyExistsInStoresAsync(key, nullptr, value, EqualityFunction));
   }

   BOOST_AUTO_TEST_CASE(AddUpdate_SingleTransaction_ShouldSucceed)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);
      KBuffer::SPtr updatedValue = ToBuffer(7);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, updatedValue, DefaultTimeout, CancellationToken::None));
         SyncAwait(VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, updatedValue, EqualityFunction));
         SyncAwait(tx->CommitAsync());
      }

      SyncAwait(VerifyKeyExistsInStoresAsync(key, nullptr, updatedValue, EqualityFunction));
   }

   BOOST_AUTO_TEST_CASE(AddDelete_SingleTransaction_ShouldSucceed)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None));
         SyncAwait(tx->CommitAsync());
      }

      SyncAwait(VerifyKeyDoesNotExistInStoresAsync(key));
   }

   BOOST_AUTO_TEST_CASE(AddDeleteAdd_SingleTransaction_ShouldSucceed)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None));
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
         SyncAwait(tx->CommitAsync());
      }

      SyncAwait(VerifyKeyExistsInStoresAsync(key, nullptr, value, EqualityFunction));
   }

   BOOST_AUTO_TEST_CASE(NoAdd_Delete_ShouldFail)
   {
      KBuffer::SPtr key = ToBuffer(5);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
         SyncAwait(Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None));
         SyncAwait(tx->AbortAsync());
      }

      SyncAwait(VerifyKeyDoesNotExistInStoresAsync(key));
   }

   BOOST_AUTO_TEST_CASE(AddDeleteUpdate_SingleTransaction_ShouldFail)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);
      KBuffer::SPtr updateValue = ToBuffer(7);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         bool result_remove = SyncAwait(Store->ConditionalRemoveAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None));
         VERIFY_ARE_EQUAL(result_remove, true);
         SyncAwait(VerifyKeyDoesNotExistAsync(*Store, *tx->StoreTransactionSPtr, key));
         bool result_update = SyncAwait(Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, updateValue, DefaultTimeout, CancellationToken::None));
         VERIFY_ARE_EQUAL(result_update, false);
         SyncAwait(tx->AbortAsync());
      }

      SyncAwait(VerifyKeyDoesNotExistInStoresAsync(key));
   }

   BOOST_AUTO_TEST_CASE(AddAdd_SingleTransaction_ShouldFail)
   {
      bool secondAddFailed = false;

      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);

      WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
      SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));

      try
      {
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
      }
      catch (ktl::Exception &)
      {
         // todo once status codes are fixed, fix this as well
         secondAddFailed = true;
      }

      CODING_ERROR_ASSERT(secondAddFailed == true);
      SyncAwait(tx->AbortAsync());
   }

   BOOST_AUTO_TEST_CASE(MultipleAdds_SingleTransaction_ShouldSucceed)
   {
       {
           WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();

           for (int i = 0; i < 10; i++)
           {
               SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, ToBuffer(i), ToBuffer(i), DefaultTimeout, CancellationToken::None));
           }

           SyncAwait(tx->CommitAsync());
       }

      for (int i = 0; i < 10; i++)
      {
         KBuffer::SPtr expectedValue = ToBuffer(i);
         SyncAwait(VerifyKeyExistsInStoresAsync(ToBuffer(i), nullptr, expectedValue, EqualityFunction));
      }
   }

   BOOST_AUTO_TEST_CASE(MultipleUpdates_SingleTransaction_ShouldSucceed)
   {
       KBuffer::SPtr key = ToBuffer(5);

       {
           WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();

           KBuffer::SPtr value = ToBuffer(0);

           SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));

           for (int i = 0; i < 10; i++)
           {
               SyncAwait(Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, ToBuffer(i), DefaultTimeout, CancellationToken::None));
           }

           SyncAwait(tx->CommitAsync());
       }

      KBuffer::SPtr expectedValue = ToBuffer(9);
      SyncAwait(VerifyKeyExistsInStoresAsync(key, nullptr, expectedValue, EqualityFunction));
   }

   BOOST_AUTO_TEST_CASE(AddUpdate_DifferentTransactions_ShouldSucceed)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(5);
      KBuffer::SPtr updateValue = ToBuffer(6);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(VerifyKeyExistsAsync(*Store, *tx1->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
         SyncAwait(tx1->CommitAsync());
      }

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
         SyncAwait(Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, updateValue, DefaultTimeout, CancellationToken::None));
         SyncAwait(tx2->CommitAsync());
      }

      SyncAwait(VerifyKeyExistsInStoresAsync(key, nullptr, updateValue, EqualityFunction));
   }

   BOOST_AUTO_TEST_CASE(AddDelete_DifferentTransactions_ShouldSucceed)
   {
       KBuffer::SPtr key = ToBuffer(5);

       {
           WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
           KBuffer::SPtr value = ToBuffer(5);

           SyncAwait(Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
           SyncAwait(VerifyKeyExistsAsync(*Store, *tx1->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
           SyncAwait(tx1->CommitAsync());
       }

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
         bool result = SyncAwait(Store->ConditionalRemoveAsync(*tx2->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None));
         CODING_ERROR_ASSERT(result == true);
         SyncAwait(tx2->CommitAsync());
      }

      SyncAwait(VerifyKeyDoesNotExistInStoresAsync(key));
   }

   BOOST_AUTO_TEST_CASE(AddUpdateRead_DifferentTransactions_ShouldSucceed)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(5);
      KBuffer::SPtr updateValue = ToBuffer(7);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(VerifyKeyExistsAsync(*Store, *tx1->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
         SyncAwait(tx1->CommitAsync());
      }

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
         bool res = SyncAwait(Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, updateValue, DefaultTimeout, CancellationToken::None));
         CODING_ERROR_ASSERT(res == true);
         SyncAwait(tx2->CommitAsync());
      }

      SyncAwait(VerifyKeyExistsInStoresAsync(key, nullptr, updateValue, EqualityFunction));
   }

   BOOST_AUTO_TEST_CASE(AddDeleteUpdate_DifferentTransactions_ShouldFail)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(5);
      KBuffer::SPtr updateValue = ToBuffer(7);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(VerifyKeyExistsAsync(*Store, *tx1->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
         SyncAwait(tx1->CommitAsync());
      }

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
         bool result = SyncAwait(Store->ConditionalRemoveAsync(*tx2->StoreTransactionSPtr, key, DefaultTimeout, CancellationToken::None));
         CODING_ERROR_ASSERT(result == true);
         SyncAwait(tx2->CommitAsync());
      }

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx3 = CreateWriteTransaction();
         bool res = SyncAwait(Store->ConditionalUpdateAsync(*tx3->StoreTransactionSPtr, key, updateValue, DefaultTimeout, CancellationToken::None));
         CODING_ERROR_ASSERT(res == false);
         SyncAwait(tx3->AbortAsync());
      }

      SyncAwait(VerifyKeyDoesNotExistInStoresAsync(key));
   }

   BOOST_AUTO_TEST_CASE(MultipleAdds_MultipleTransactions_ShouldSucceed)
   {
      for (int i = 0; i < 10; i++)
      {
         KBuffer::SPtr key = ToBuffer(i);
         KBuffer::SPtr value = ToBuffer(i);

         {
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
            SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
            SyncAwait(tx->CommitAsync());
         }
      }

      {
          for (ULONG m = 0; m < Stores->Count(); m++)
          {
              WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction(*(*Stores)[m]);
              for (int i = 0; i < 10; i++)
              {
                  SyncAwait(VerifyKeyExistsAsync(*(*Stores)[m], *tx->StoreTransactionSPtr, ToBuffer(i), nullptr, ToBuffer(i), EqualityFunction));
              }

              SyncAwait(tx->AbortAsync());
          }
      }
   }

   BOOST_AUTO_TEST_CASE(MultipleAddsUpdates_MultipleTransactions_ShouldSucceed)
   {
      for (int i = 0; i < 10; i++)
      {
         KBuffer::SPtr key = ToBuffer(i);
         KBuffer::SPtr value = ToBuffer(i);

         {
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
            SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
            SyncAwait(tx->CommitAsync());
         }
      }

      for (int i = 0; i < 10; i++)
      {
         KBuffer::SPtr key = ToBuffer(i);
         KBuffer::SPtr value = ToBuffer(i+10);

         {
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
            SyncAwait(Store->ConditionalUpdateAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
            SyncAwait(tx->CommitAsync());
         }
      }

      {
          for (ULONG m = 0; m < Stores->Count(); m++)
          {
              WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction(*(*Stores)[m]);
              for (int i = 0; i < 10; i++)
              {
                  SyncAwait(VerifyKeyExistsAsync(*(*Stores)[m], *tx->StoreTransactionSPtr, ToBuffer(i), nullptr, ToBuffer(i+10), EqualityFunction));
              }

              SyncAwait(tx->AbortAsync());
          }
      }
   }

   BOOST_AUTO_TEST_CASE(Add_Abort_ShouldSucceed)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(5);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         KeyValuePair<LONG64, KBuffer::SPtr> kvpair(-1, ToBuffer(0));
         SyncAwait(Store->ConditionalGetAsync(*tx->StoreTransactionSPtr, key, DefaultTimeout, kvpair, CancellationToken::None));
         CODING_ERROR_ASSERT(*kvpair.Value == *value);
         SyncAwait(tx->AbortAsync());
      }

      SyncAwait(VerifyKeyDoesNotExistInStoresAsync(key));
   }

   BOOST_AUTO_TEST_CASE(AddAdd_SameKeyOnConcurrentTransaction_ShouldTimeout)
   {
      bool hasFailed = false;

      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));

         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
         try
         {
            SyncAwait(Store->AddAsync(*tx2->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         }
         catch (ktl::Exception & e)
         {
            hasFailed = true;
            CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_TIMEOUT);
         }

         CODING_ERROR_ASSERT(hasFailed);

         SyncAwait(tx2->AbortAsync());
         SyncAwait(tx1->AbortAsync());
      }

      SyncAwait(VerifyKeyDoesNotExistInStoresAsync(key));
   }

   BOOST_AUTO_TEST_CASE(UpdateRead_SameKeyOnConcurrentTransaction_ShouldTimeout)
   {
      bool hasFailed = false;

      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(tx->CommitAsync());
      }

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
         SyncAwait(Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, key, ToBuffer(8), DefaultTimeout, CancellationToken::None));

         {
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
            tx2->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::ReadRepeatable;
            try
            {
                SyncAwait(VerifyKeyExistsAsync(*Store, *tx2->StoreTransactionSPtr, key, nullptr, ToBuffer(8), EqualityFunction));
            }
            catch (ktl::Exception & e)
            {
               hasFailed = true;
               CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_TIMEOUT);
            }

            CODING_ERROR_ASSERT(hasFailed);

            SyncAwait(tx2->AbortAsync());
            SyncAwait(tx1->AbortAsync());
         }
      }

      SyncAwait(VerifyKeyExistsInStoresAsync(key, nullptr, value, EqualityFunction));
   }

   BOOST_AUTO_TEST_CASE(WriteStore_NotPrimary_ShouldFail)
   {
      Replicator->SetWriteStatus(FABRIC_SERVICE_PARTITION_ACCESS_STATUS_NOT_PRIMARY);
      bool hasFailed = false;

      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);

      {
          WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
          try
          {
              SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
          }
          catch (ktl::Exception & e)
          {
              hasFailed = true;
              CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_NOT_PRIMARY);
          }

          CODING_ERROR_ASSERT(hasFailed);
          SyncAwait(tx->AbortAsync());
      }

      SyncAwait(VerifyKeyDoesNotExistInStoresAsync(key));
   }

   BOOST_AUTO_TEST_CASE(ReadStore_NotReadable_ShouldFail)
   {
      bool hasFailed = false;

      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);

      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(tx->CommitAsync());
      }

      Replicator->SetReadStatus(FABRIC_SERVICE_PARTITION_ACCESS_STATUS_RECONFIGURATION_PENDING);

      WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
      try
      {
          SyncAwait(VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
      }
      catch (ktl::Exception & e)
      {
         hasFailed = true;
         CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_NOT_READABLE);
      }
      SyncAwait(tx->AbortAsync());
      CODING_ERROR_ASSERT(hasFailed);
   }

   BOOST_AUTO_TEST_CASE(ReadStore_ActiveSecondary_NotReadable_ShouldFail)
   {
       bool hasFailed = false;

       KBuffer::SPtr key = ToBuffer(5);
       KBuffer::SPtr value = ToBuffer(6);

       {
           WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
           SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
           SyncAwait(tx->CommitAsync());
       }
        
       Replicator->Role = FABRIC_REPLICA_ROLE_ACTIVE_SECONDARY;
       Replicator->SetReadStatus(FABRIC_SERVICE_PARTITION_ACCESS_STATUS_RECONFIGURATION_PENDING);
       Replicator->SetReadable(false);

       WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
       try
       {
           SyncAwait(VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
       }
       catch (ktl::Exception & e)
       {
           hasFailed = true;
           CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_NOT_READABLE);
       }
       SyncAwait(tx->AbortAsync());
       CODING_ERROR_ASSERT(hasFailed);
   }

   BOOST_AUTO_TEST_CASE(ReadStore_ActiveSecondary_IsReadable_NotSnapshotTxn_ShouldFail)
   {
       bool hasFailed = false;

       KBuffer::SPtr key = ToBuffer(5);
       KBuffer::SPtr value = ToBuffer(6);

       {
           WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
           SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
           SyncAwait(tx->CommitAsync());
       }
        
       Replicator->Role = FABRIC_REPLICA_ROLE_ACTIVE_SECONDARY;
       Replicator->SetReadStatus(FABRIC_SERVICE_PARTITION_ACCESS_STATUS_RECONFIGURATION_PENDING);
       Replicator->SetReadable(true);

       WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
       tx->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::ReadRepeatable;
       try
       {
           SyncAwait(VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
       }
       catch (ktl::Exception & e)
       {
           hasFailed = true;
           CODING_ERROR_ASSERT(e.GetStatus() == SF_STATUS_NOT_READABLE);
       }
       SyncAwait(tx->AbortAsync());
       CODING_ERROR_ASSERT(hasFailed);
   }

   BOOST_AUTO_TEST_CASE(ReadStore_ActiveSecondary_IsReadable_SnapshotTxn_ShouldSucceed)
   {
       KBuffer::SPtr key = ToBuffer(5);
       KBuffer::SPtr value = ToBuffer(6);

       {
           WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
           SyncAwait(Store->AddAsync(*tx->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
           SyncAwait(tx->CommitAsync());
       }
        
       Replicator->Role = FABRIC_REPLICA_ROLE_ACTIVE_SECONDARY;
       Replicator->SetReadStatus(FABRIC_SERVICE_PARTITION_ACCESS_STATUS_RECONFIGURATION_PENDING);
       Replicator->SetReadable(true);

       {
           WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction();
           tx->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::Snapshot;
           SyncAwait(VerifyKeyExistsAsync(*Store, *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
           SyncAwait(tx->AbortAsync());
       }
   }

   BOOST_AUTO_TEST_CASE(SnapshotRead_FromSnapshotContainer_MovedFromDifferentialState)
   {
      KBuffer::SPtr key = ToBuffer(5);
      KBuffer::SPtr value = ToBuffer(6);

      // Add
      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
         SyncAwait(Store->AddAsync(*tx1->StoreTransactionSPtr, key, value, DefaultTimeout, CancellationToken::None));
         SyncAwait(tx1->CommitAsync());
      }

      // Start the snapshot transaction here
      KArray<WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr> storesTransactions(GetAllocator());

      for (ULONG i = 0; i < Stores->Count(); i++)
      {
          WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction(*(*Stores)[i]);
          tx->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::Snapshot;
          storesTransactions.Append(tx);
          SyncAwait(VerifyKeyExistsAsync(*(*Stores)[i], *tx->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
      }

      // Update causes entries to previous version.
      {
         WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
         SyncAwait(Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, ToBuffer(7), DefaultTimeout, CancellationToken::None));
         SyncAwait(tx2->CommitAsync());
      }

      // Update again to move entries to snapshot container
      {
          WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx2 = CreateWriteTransaction();
          SyncAwait(Store->ConditionalUpdateAsync(*tx2->StoreTransactionSPtr, key, ToBuffer(8), DefaultTimeout, CancellationToken::None));
          SyncAwait(tx2->CommitAsync());
      }

      SyncAwait(VerifyKeyExistsInStoresAsync(key, nullptr, ToBuffer(8), EqualityFunction));

      for (ULONG i = 0; i < storesTransactions.Count(); i++)
      {
          SyncAwait(VerifyKeyExistsAsync(*storesTransactions[i]->StateProviderSPtr, *storesTransactions[i]->StoreTransactionSPtr, key, nullptr, value, EqualityFunction));
          SyncAwait(storesTransactions[i]->AbortAsync());
      }

      storesTransactions.Clear();
   }

   BOOST_AUTO_TEST_CASE(SnapshotRead_FromConsolidatedState)
   {
      ULONG32 count = 4;

      for (ULONG32 idx = 0; idx < count; idx++)
      {
         {
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
            SyncAwait(Store->AddAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx), DefaultTimeout, CancellationToken::None));
            SyncAwait(tx1->CommitAsync());
         }
      }

      // Start the snapshot transaction here
      KArray<WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr> storesTransactions(GetAllocator());

      for (ULONG i = 0; i < Stores->Count(); i++)
      {
          WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction(*(*Stores)[i]);
          tx->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::Snapshot;
          storesTransactions.Append(tx);
          for (ULONG32 idx = 0; idx < count; idx++)
          {
              SyncAwait(VerifyKeyExistsAsync(*(*Stores)[i], *tx->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction));
          }
      }

      Checkpoint();

      // Update after checkpoint.
      for (ULONG32 idx = 0; idx < count; idx++)
      {
         {
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
            SyncAwait(Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx + 10), DefaultTimeout, CancellationToken::None));
            SyncAwait(tx1->CommitAsync());
         }
      }

      for (ULONG32 idx = 0; idx < count; idx++)
      {
         SyncAwait(VerifyKeyExistsInStoresAsync(ToBuffer(idx), nullptr, ToBuffer(idx + 10), EqualityFunction));
      }

      for (ULONG i = 0; i < storesTransactions.Count(); i++)
      {
          for (ULONG32 idx = 0; idx < count; idx++)
          {
              SyncAwait(VerifyKeyExistsAsync(*storesTransactions[i]->StateProviderSPtr, *storesTransactions[i]->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction));
          }

          SyncAwait(storesTransactions[i]->AbortAsync());
      }

      storesTransactions.Clear();
   }

   BOOST_AUTO_TEST_CASE(SnapshotRead_FromSnapshotContainer_MovedFromDifferential_DuringConsolidation)
   {
      ULONG32 count = 4;
      for (ULONG32 idx = 0; idx < count; idx++)
      {
         {
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
            SyncAwait(Store->AddAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx), DefaultTimeout, CancellationToken::None));
            SyncAwait(tx1->CommitAsync());
         }
      }

      // Start the snapshot transaction here
      KArray<WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr> storesTransactions(GetAllocator());

      for (ULONG i = 0; i < Stores->Count(); i++)
      {
          WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction(*(*Stores)[i]);
          tx->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::Snapshot;
          storesTransactions.Append(tx);
          for (ULONG32 idx = 0; idx < count; idx++)
          {
              SyncAwait(VerifyKeyExistsAsync(*(*Stores)[i], *tx->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction));
          }
      }

      for (ULONG32 idx = 0; idx < count; idx++)
      {
         {
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
            SyncAwait(Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx + 10), DefaultTimeout, CancellationToken::None));
            SyncAwait(tx1->CommitAsync());
         }
      }

      // Read updated value to validate.
      for (ULONG32 idx = 0; idx < count; idx++)
      {
         SyncAwait(VerifyKeyExistsInStoresAsync(ToBuffer(idx), nullptr, ToBuffer(idx + 10), EqualityFunction));
      }

      Checkpoint();

      for (ULONG32 idx = 0; idx < count; idx++)
      {
         {
            WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
            SyncAwait(Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx + 20), DefaultTimeout, CancellationToken::None));
            SyncAwait(tx1->CommitAsync());
         }
      }

      // Read updated value to validate.
      for (ULONG32 idx = 0; idx < count; idx++)
      {
         SyncAwait(VerifyKeyExistsInStoresAsync(ToBuffer(idx), nullptr, ToBuffer(idx + 20), EqualityFunction));
      }

      for (ULONG i = 0; i < storesTransactions.Count(); i++)
      {
          for (ULONG32 idx = 0; idx < count; idx++)
          {
              SyncAwait(VerifyKeyExistsAsync(*storesTransactions[i]->StateProviderSPtr, *storesTransactions[i]->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction));
          }

          SyncAwait(storesTransactions[i]->AbortAsync());
      }

      storesTransactions.Clear();
   }

   BOOST_AUTO_TEST_CASE(SnapshotRead_FromSnapshotContainer_MovedFromConsolidatedState)
   {
       ULONG32 count = 4;
       for (ULONG32 idx = 0; idx < count; idx++)
       {
           {
               WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
               SyncAwait(Store->AddAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx), DefaultTimeout, CancellationToken::None));
               SyncAwait(tx1->CommitAsync());
           }
       }

       // Start the snapshot transaction here
       KArray<WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr> storesTransactions(GetAllocator());

       for (ULONG i = 0; i < Stores->Count(); i++)
       {
           WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx = CreateWriteTransaction(*(*Stores)[i]);
           tx->StoreTransactionSPtr->ReadIsolationLevel = StoreTransactionReadIsolationLevel::Snapshot;
           storesTransactions.Append(tx);
           for (ULONG32 idx = 0; idx < count; idx++)
           {
               SyncAwait(VerifyKeyExistsAsync(*(*Stores)[i], *tx->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction));
           }
       }

       Checkpoint();

       for (ULONG32 idx = 0; idx < count; idx++)
       {
           {
               WriteTransaction<KBuffer::SPtr, KBuffer::SPtr>::SPtr tx1 = CreateWriteTransaction();
               SyncAwait(Store->ConditionalUpdateAsync(*tx1->StoreTransactionSPtr, ToBuffer(idx), ToBuffer(idx + 10), DefaultTimeout, CancellationToken::None));
               SyncAwait(tx1->CommitAsync());
           }
       }

       Checkpoint();

       // Read updated value to validate.
       for (ULONG32 idx = 0; idx < count; idx++)
       {
           SyncAwait(VerifyKeyExistsInStoresAsync(ToBuffer(idx), nullptr, ToBuffer(idx + 10), EqualityFunction));
       }

       for (ULONG i = 0; i < storesTransactions.Count(); i++)
       {
           for (ULONG32 idx = 0; idx < count; idx++)
           {
               SyncAwait(VerifyKeyExistsAsync(*storesTransactions[i]->StateProviderSPtr, *storesTransactions[i]->StoreTransactionSPtr, ToBuffer(idx), nullptr, ToBuffer(idx), EqualityFunction));
           }

           SyncAwait(storesTransactions[i]->AbortAsync());
       }

       storesTransactions.Clear();
   }

   BOOST_AUTO_TEST_SUITE_END()
}
