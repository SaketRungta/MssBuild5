// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "MssSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(MultiplayerSessionSubsystemLog, Log, All);

#define SETTING_NUMPLAYERSREQUIRED FName("NumPlayers") 
#define SETTING_FILTERSEED FName("FilterSeed")
#define SETTING_FILTERSEED_VALUE 94311 

#pragma region Custom Delegates

/**
 * Custom delegates for the classes invoking this subsystem to bind to
 */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerSessionsOnCreateSessionComplete, bool, bWasSuccessful);
/** FOnlineSessionSearchResult is not UCLASS so we cannot use DYNAMIC keyword here */
DECLARE_MULTICAST_DELEGATE_TwoParams(FMultiplayerSessionsOnFindSessionsComplete, const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);
/** EOnJoinSessionCompleteResult is not UCLASS so we cannot use DYNAMIC keyword here */
DECLARE_MULTICAST_DELEGATE_OneParam(FMultiplayerSessionsOnJoinSessionsComplete, EOnJoinSessionCompleteResult::Type Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerSessionsOnDestroySessionComplete, bool, bWasSuccessful);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMultiplayerSessionsOnStartSessionComplete, bool, bWasSuccessful);

#pragma endregion Custom Delegates

/**
 * Structure to store all the settings to be set while creating a session
 ******************************************************************************************/
USTRUCT(Blueprintable, BlueprintType)
struct FTempCustomSessionSettings
{
	GENERATED_BODY()

	/** Name of the map that user has selected */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString MapName = FString("");

	/** Game mode selected by the user */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString GameMode = FString("");

	/** Numbers of players session will host */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Players = FString("");
};

/**
 * Class to handle all the session operations
 * Being a subsystem of game instance this can be called from anywhere
 ******************************************************************************************/
UCLASS(ClassGroup = (Subsystem))
class MULTIPLAYERSESSIONSSUBSYSTEM_API UMssSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	/** Default Constructor */
	UMssSubsystem();
	
	virtual void Deinitialize() override;

#pragma region Session Operations

	/**
	 * Creates a session for the host to join
	 *
	 * @param InCustomSessionSettings: Custom session settings to create the session with
	 */
	void CreateSession(const FTempCustomSessionSettings& InCustomSessionSettings);

	/** Finds sessions for the client to join to */
	void FindSessions();

	void CancelFindSessions();
	
private:
	bool bFindSessionsInProgress = false;
	
public:
	/**
	 * Join the session requested by the client
	 *
	 *  @param InSessionToJoin: Passed by the client after selecting the appropriate session he wishes to join
	 */
	void JoinSessions(FOnlineSessionSearchResult& InSessionToJoin);

	/** Destroys the currently active session */
	void DestroySession();

	/** Starts the actual session */
	void StartSession();
	
#pragma endregion Session Operations

#pragma region Custom Delegates Declaration

	/**
	 * Custom delegates for classes invoking this subsystem to bind to
	 */
	FMultiplayerSessionsOnCreateSessionComplete MultiplayerSessionsOnCreateSessionComplete;
	FMultiplayerSessionsOnFindSessionsComplete MultiplayerSessionsOnFindSessionsComplete;
	FMultiplayerSessionsOnJoinSessionsComplete MultiplayerSessionsOnJoinSessionsComplete;
	FMultiplayerSessionsOnDestroySessionComplete MultiplayerSessionsOnDestroySessionComplete;
	FMultiplayerSessionsOnStartSessionComplete MultiplayerSessionsOnStartSessionComplete;
	
#pragma endregion Custom Delegates Declaration
	
private:
	void HandleAppExit();

	/**
	 * Generates and returns a random unique code to create a session with
	 * For the clients to later search and join a session with this unique code
	 *
	 * Appends the time in minutes, seconds and milliseconds and returns it as session code
	 *
	 * NOTE: Under extremely rare circumstances the generated code may not be unique and there might be a session with same codes
	 * 
	 * @return an FSting with a random unique code
	 */
	FString GenerateSessionUniqueCode() const;

	/**
	 * This variable acts an access point to OnlineSubsystem's session interface
	 *
	 * Using this variable only I will be able to call for creation, destruction, joining and finding of sessions
	 * Initialized in the constructor
	 */
	IOnlineSessionPtr SessionInterface;

	/** Stores the last created session search to get the search results */
	TSharedPtr<FOnlineSessionSearch> LastCreatedSessionSearch;
	
#pragma region Session Complete Delegates

	/**
	 * Delegates for session interface to bind to so that we can get a callback on any session operation completion
	 */
	
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FDelegateHandle CreateSessionCompleteDelegateHandle;
	
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	FDelegateHandle FindSessionsCompleteDelegateHandle;
	
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
	FDelegateHandle JoinSessionCompleteDelegateHandle;
	
	FOnDestroySessionCompleteDelegate DestroySessionCompleteDelegate;
	FDelegateHandle DestroySessionCompleteDelegateHandle;
	
	FOnStartSessionCompleteDelegate StartSessionCompleteDelegate;
	FDelegateHandle StartSessionCompleteDelegateHandle;
		
#pragma endregion Session Complete Delegates
	
#pragma region Session Operations On Completion Delegates Callbacks
	
	/** Called when a session is successfully created */
	void OnCreateSessionCompleteCallback(FName SessionName, bool bWasSuccessful);

	/** Called when sessions with given session settings are found */
	void OnFindSessionsCompleteCallback(bool bWasSuccessful);

	/** Called when a session is joined */
	void OnJoinSessionCompleteCallback(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

	/** Called when a session is destroyed */
	void OnDestroySessionCompleteCallback(FName SessionName, bool bWasSuccessful);

	/** Called when a session is destroyed */
	void OnStartSessionCompleteCallback(FName SessionName, bool bWasSuccessful);
	
#pragma endregion Session Operations On Completion Delegates Callbacks

	/** True when user requests to create a new session but the last created session is already active */
	bool bCreateSessionOnDestroy = false;

	/**
	 * When a session is already active player selects to create a new session then this will store the settings
	 * As we will have to destroy the active session and then create a new one 
	 */
	FTempCustomSessionSettings SessionSettingsForTheSessionToCreateAfterDestruction;
	
	int32 JoinRetryCounter = 0;
	
	int32 MaxJoinRetries = 1;
	
	bool IsSessionInState(EOnlineSessionState::Type State) const;
};
