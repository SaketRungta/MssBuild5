// Copyright (c) 2025 The Unreal Guy. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystem/MssSubsystem.h"
#include "MssHUD.generated.h"

class UMssSessionDataWidget;

/**
 * HUD class implements the multiplayer sessions subsystem
 * Requests for all the multiplayer functionality and handles the callbacks
 ******************************************************************************************/
UCLASS(Blueprintable, BlueprintType, ClassGroup = (Widgets))
class MULTIPLAYERSESSIONSSUBSYSTEM_API UMssHUD : public UUserWidget
{
	GENERATED_BODY()
	
public:
	/** Default constructor */
	UMssHUD(const FObjectInitializer& ObjectInitializer);
	
protected:
	/** Function to initialize the widget */
	virtual bool Initialize() override;

private:
	
#pragma region Multiplayer Sessions Callbacks

	/**
	 * Callback from subsystem binding after completing session creation operation
	 *
	 * @param bWasSuccessful: True if session creation was successful
	 */
	UFUNCTION()
	void OnSessionCreatedCallback(bool bWasSuccessful);
	
	/**
	 * Callback from subsystem binding after completing finding sessions operation
	 *
	 * @param SessionResults: True if session creation was successful
	 * @param bWasSuccessful: True when the operation was successful
	 */
	void OnSessionsFoundCallback(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful);

	/**
	 * Callback from subsystem binding after completing session joining operation
	 *
	 * @param Result: Result of the session joining operation
	 */
	void OnSessionJoinedCallback(EOnJoinSessionCompleteResult::Type Result);

	/**
	 * Callback from subsystem binding after completing session destruction operation
	 * 
	 * @param bWasSuccessful: true if session destruction was successful
	 */
	UFUNCTION()
	void OnSessionDestroyedCallback(bool bWasSuccessful);

	/**
	 * Callback from subsystem binding after completing session started operation
	 * 
	 * @param bWasSuccessful: true if session was started successfully
	 */
	UFUNCTION()
	void OnSessionStartedCallback(bool bWasSuccessful);
	
#pragma endregion Multiplayer Sessions Callbacks

	/**
	 * Called to request the MssSubsystem to host the game with the given settings
	 * 
	 * @param InSessionSettings settings to host the session with
	 */
	UFUNCTION(BlueprintCallable, Category = "MssHUD")
	void HostGame(const FTempCustomSessionSettings& InSessionSettings);
	
	/**
	 * Called to search a session with the given session settings
	 * 
	 * @param InSessionSettings settings to find the session with
	 */
	UFUNCTION(BlueprintCallable, Category = "MssHUD")
	void FindGame(const FTempCustomSessionSettings& InSessionSettings);

	/**
	 * Called when user enters any session code he wishes to join
	 * Function requests the MssSubsystem to find all the active session
	 * Then it checks if any session is hosted with the entered code and then requests to join it 
	 * 
	 * @param InSessionCode: Session code entered by the user
	 */
	UFUNCTION(BlueprintCallable, Category = "MssHUD")
	void EnterCode(const FText& InSessionCode);

	/**
	 * When sessions are found by MssSubsystem this function is called if player has requested to join the session via code
	 * This function then filters the result and checks is a session exists with given session code and joins it
	 * 
	 * @param SessionSearchResults 
	 */
	void JoinSessionViaSessionCode(const TArray<FOnlineSessionSearchResult>& SessionSearchResults);

	/**
	 * When user is finding sessions then this function is called to display all the active sessions
	 * It also filters out session results and displays only relevant session that user wishes to join
	 * 
	 * @param SessionSearchResults 
	 */
	void AddSessionSearchResultsToScrollBox(const TArray<FOnlineSessionSearchResult>& SessionSearchResults);
	
	/**
	 * Filters and returns the entered session code so that the code does not exceed the max limit
	 * Also checks if all the characters entered are digits only
	 *
	 * @param InCode: The code entered by the user
	 * @return FText: The filtered out code that has to be displayed on the text box
	 */
	UFUNCTION(BlueprintCallable, Category = "MssHUD")
	FText OnEnteredSessionCodeChanged(const FText& InCode);
	
	/** Subsystem that handles all the multiplayer functionality */
	TObjectPtr<UMssSubsystem> MssSubsystem;

	/** Path to the lobby map, we will travel to this map after creating a session successfully */
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Sessions Subsystem")
	FString LobbyMapPath = FString("");

	/** True when user has requested to join via session code */
	bool bJoinSessionViaCode = false;

	/** The session code that user wishes to join */
	FString SessionCodeToJoin = "";

	/** Session settings the user wishes to filter from */
	FTempCustomSessionSettings FilterSessionSettings;

	/** Widget class to add to the session data scroll box */
	UPROPERTY(EditDefaultsOnly, Category = "Multiplayer Sessions Subsystem")
	TSubclassOf<UMssSessionDataWidget> SessionDataWidgetClass;
	
public:
	/**
	 * Called from UMssSessionDataWidget::OnJoinSessionButtonClicked when user clicks on the join button for the session he wishes to join
	 *
	 * @paran InSessionToJoin: The session user wishes to join
	 */
	void JoinTheGivenSession(FOnlineSessionSearchResult& InSessionToJoin);

	/** Displays a message on the HUD, if it is an error then also sets the message close to be visible
	 * 
	 * @param InMessage: Message to display
	 * @param bIsErrorMessage: True when it is an error message to that close button can be displayed
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void ShowMessage(const FString& InMessage, bool bIsErrorMessage = false);

	/**
	 * Adds a session data widget to the scroll box of the session result widget
	 * Hides if any message is displayed and makes the widget switcher show the session result widget
	 * 
	 * @param InSessionDataWidget: The widget to add
	 */
	UFUNCTION(BlueprintImplementableEvent)
	void AddSessionDataWidget(UMssSessionDataWidget* InSessionDataWidget);

	/** Calls to clear the session data scroll box, as we want to show the fresh set of search data */
	UFUNCTION(BlueprintImplementableEvent)
	void ClearSessionDataScrollBox();

};
