#pragma once

#include <list>
#include <vector>
#include <unordered_map>
#include <memory>
#include <mutex>

#include "IWalletLegacy.h"
#include "INode.h"
#include "wallet/WalletErrors.h"
#include "wallet/WalletAsyncContextCounter.h"
#include "ObserverManager.h"
#include "core/trans/TransactionExtra.h"
#include "base/CryptoNoteFormatUtils.h"
#include "core/Currency.h"
#include "log/ConsoleLogger.h"
#include "WalletUserTransactionsCache.h"
#include "WalletUnconfirmedTransactions.h"

#include "WalletTransactionSender.h"
#include "WalletRequest.h"

#include "transfers/BlockchainSynchronizer.h"
#include "transfers/TransfersSynchronizer.h"

namespace CryptoNote {

class SyncStarter;

class WalletLegacy : 
  public IWalletLegacy, 
  IBlockchainSynchronizerObserver,  
  ITransfersObserver {

public:
  WalletLegacy(const CryptoNote::Currency& currency, INode& node, Logging::ILogger& loggerGroup);
  virtual ~WalletLegacy();

  virtual void addObserver(IWalletLegacyObserver* observer) override;
  virtual void removeObserver(IWalletLegacyObserver* observer) override;

  virtual void initAndGenerate(const std::string& password) override;
  virtual void initAndGenerateDeterministic(const std::string& password) override;
  virtual void initAndLoad(std::istream& source, const std::string& password) override;
  virtual void initWithKeys(const AccountKeys& accountKeys, const std::string& password) override;
  virtual void shutdown() override;
  virtual void reset() override;

  virtual Crypto::SecretKey generateKey(const std::string& password, const Crypto::SecretKey& recovery_param = Crypto::SecretKey(), 
bool recover = false, bool two_random = false) override;

  virtual void save(std::ostream& destination, bool saveDetailed = true, bool saveCache = true) override;

  virtual std::error_code changePassword(const std::string& oldPassword, const std::string& newPassword) override;

  virtual std::string getAddress() override;

  virtual uint64_t actualBalance() override;
  virtual uint64_t pendingBalance() override;
  virtual uint64_t dustBalance() override;
  virtual uint64_t actualDepositBalance() override;
  virtual uint64_t pendingDepositBalance() override;

  virtual size_t getTransactionCount() override;
  virtual size_t getTransferCount() override;
  virtual size_t getDepositCount() override;

  virtual TransactionId findTransactionByTransferId(TransferId transferId) override;

  virtual bool getTransaction(TransactionId transactionId, WalletLegacyTransaction& transaction) override;
  virtual bool getTransfer(TransferId transferId, WalletLegacyTransfer& transfer) override;
  virtual bool getDeposit(DepositId depositId, Deposit& deposit) override;
  virtual std::vector<Payments> getTransactionsByPaymentIds(const std::vector<PaymentId>& paymentIds) const override;

  virtual TransactionId sendTransaction(const WalletLegacyTransfer& transfer,
                                        uint64_t fee,
                                        const std::string& extra = "",
                                        uint64_t mixIn = 0,
                                        uint64_t unlockTimestamp = 0);
  virtual TransactionId sendTransaction(const std::vector<WalletLegacyTransfer>& transfers,
                                        uint64_t fee,
                                        const std::string& extra = "",
                                        uint64_t mixIn = 0,
                                        uint64_t unlockTimestamp = 0);
  virtual TransactionId sendDustTransaction(const WalletLegacyTransfer& transfer, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) override;
virtual TransactionId sendDustTransaction(const std::vector<WalletLegacyTransfer>& transfers, uint64_t fee, const std::string& extra = "", uint64_t mixIn = 0, uint64_t unlockTimestamp = 0) override;
  virtual TransactionId deposit(uint32_t term, uint64_t amount, uint64_t fee, uint64_t mixIn = 0) override;
  virtual TransactionId withdrawDeposits(const std::vector<DepositId>& depositIds, uint64_t fee) override;
  virtual std::error_code cancelTransaction(size_t transactionId) override;

  virtual void getAccountKeys(AccountKeys& keys) override;
  virtual bool getSeed(std::string& electrum_words) override;

private:

  // IBlockchainSynchronizerObserver
  virtual void synchronizationProgressUpdated(uint32_t current, uint32_t total) override;
  virtual void synchronizationCompleted(std::error_code result) override;

  // ITransfersObserver
  virtual void onTransactionUpdated(ITransfersSubscription* object, const Crypto::Hash& transactionHash) override;
  virtual void onTransactionDeleted(ITransfersSubscription* object, const Crypto::Hash& transactionHash) override;
  virtual void onTransfersUnlocked(ITransfersSubscription* object, const std::vector<TransactionOutputInformation>& unlockedTransfers) override;
  virtual void onTransfersLocked(ITransfersSubscription* object, const std::vector<TransactionOutputInformation>& lockedTransfers) override;

  void initSync();
  void throwIfNotInitialised();

  void doSave(std::ostream& destination, bool saveDetailed, bool saveCache);
  void doLoad(std::istream& source);

  void synchronizationCallback(WalletRequest::Callback callback, std::error_code ec);
  void sendTransactionCallback(WalletRequest::Callback callback, std::error_code ec);
  void notifyClients(std::deque<std::unique_ptr<WalletLegacyEvent> >& events);
  void notifyIfBalanceChanged();
  void notifyIfDepositBalanceChanged();

  std::unique_ptr<WalletLegacyEvent> getActualDepositBalanceChangedEvent();
  std::unique_ptr<WalletLegacyEvent> getPendingDepositBalanceChangedEvent();

  std::unique_ptr<WalletLegacyEvent> getActualBalanceChangedEvent();
  std::unique_ptr<WalletLegacyEvent> getPendingBalanceChangedEvent();

  uint64_t calculateActualDepositBalance();
  uint64_t calculatePendingDepositBalance();

  uint64_t calculateActualBalance();
  uint64_t calculatePendingBalance();

  void pushBalanceUpdatedEvents(std::deque<std::unique_ptr<WalletLegacyEvent>>& eventsQueue);

  std::vector<TransactionId> deleteOutdatedUnconfirmedTransactions();
  
  std::vector<uint32_t> getTransactionHeights(std::vector<TransactionOutputInformation> transfers);

  enum WalletState
  {
    NOT_INITIALIZED = 0,
    INITIALIZED,
    LOADING,
    SAVING
  };

  WalletState m_state;
  std::mutex m_cacheMutex;
  CryptoNote::AccountBase m_account;
  std::string m_password;
  const CryptoNote::Currency& m_currency;
  INode& m_node;
  Logging::ILogger& m_loggerGroup;
  bool m_isStopping;

  std::atomic<uint64_t> m_lastNotifiedActualBalance;
  std::atomic<uint64_t> m_lastNotifiedPendingBalance;

  std::atomic<uint64_t> m_lastNotifiedActualDepositBalance;
  std::atomic<uint64_t> m_lastNotifiedPendingDepositBalance;

  BlockchainSynchronizer m_blockchainSync;
  TransfersSyncronizer m_transfersSync;
  ITransfersContainer* m_transferDetails;

  WalletUserTransactionsCache m_transactionsCache;
  std::unique_ptr<WalletTransactionSender> m_sender;

  WalletAsyncContextCounter m_asyncContextCounter;
  Tools::ObserverManager<CryptoNote::IWalletLegacyObserver> m_observerManager;

  std::unique_ptr<SyncStarter> m_onInitSyncStarter;
};

} //namespace CryptoNote
