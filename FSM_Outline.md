# Host FSM States
| State | Description | Changes from | Changes to |
| --- | --- | --- | --- |
| Pairing | Host's buzzer is listening for pairing requests from buzzers. During this time all paired buzzers flash synchronously. | Poweroff | `Standby` via button press |
| Standby | Host is not looking for responses. They could be reading a question, talking etc. | `Pairing` & `WinnerSelectionFlashing` | `ReceivingBuzzerResponses` via button press |
| ReceivingBuzzerResponses | Host is looking for responses, host's buzzer is listening for responses from player buzzers. | `Standby` | `WinnerSelectionFlashing` via first response received from player buzzer |
| WinnerSelectionFlashing | Host's buzzer and winner's player buzzer is synchronously flashing indicating they are the winner | `ReceivingBuzzerResponses` | `Standby` via button press |

**Notes:**

Pairing to add *new* buzzers should be done in the `Pairing` state. Reconnection from buzzers reconnecting should happen automatically during "a game".

# Player Buzzer FSM States
| State | Description | Changes from | Changes to |
| --- | --- | --- | --- |
| LookingToPair | Buzzer is looking for a host to pair with. If pairing is successful the buzzer will begin to flash in sync with the host. | Poweroff | `Standby` via network message from Host |
| Standby | Waiting for instructions from Host | `LookingToPair`, `ReadyToSend` & `SelectedAsWinner` | `ReadyToSend` via network messages from Host |
| ReadyToSend | Host's buzzer is listening for responses from player buzzers. The buzzer's light turns on to indicate to the player that the system is active. | `Standby` via network message from host | `Standby` via player pressing the button. |
| SelectedAsWinner | Buzzer has been sellected as winner by the host. Light will start flashing in sync with the host. | `Standby` | `Standby` via network message from Host |