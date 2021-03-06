#include "Simulation.h"

// A function that runs the simulation
int RunSim(Person _PassengerList[], Person** _PassengerLocationMatrix[], bool ShouldUpdateVisuals, Map _PlaneMap, SimulationConfig _BaseRules)
{
	bool AllSeated = false;
	clock_t OneSecWatchStart, OneSecWatchEnd;
	int RPSCount = 0;
	int ShowRPCCount = 0;
	int StepsTaken = 0;

	OneSecWatchStart = clock();

	while (!AllSeated)
	{
		AllSeated = MoveAllPassengersAndCheckIfSeated(_PassengerList, _PassengerLocationMatrix, _PlaneMap, _BaseRules);

		if (ShouldUpdateVisuals)
			UpdateVisuals(_PassengerLocationMatrix, _PlaneMap, &ShowRPCCount, &RPSCount, &OneSecWatchStart, &OneSecWatchEnd);

		StepsTaken++;
	}

	return StepsTaken;
}

// A bool function that checks if a passenger is seated and if they aren't it uses another function to move the passenger
bool MoveAllPassengersAndCheckIfSeated(Person _PassengerList[], Person** _PassengerLocationMatrix[], Map _PlaneMap, SimulationConfig _BaseRules)
{
	bool AllSeated = true;
	for (int i = 0; i < _PlaneMap.NumberOfSeats; i++)
	{
		if (_PassengerList[i].IsSeated == false)
		{
			AllSeated = false;

			PerformPassengerMove(&_PassengerList[i], _PassengerLocationMatrix, _PlaneMap, _BaseRules);

			_PassengerList[i].IsSeated = CanPassengerSit(_PassengerList[i]);
		}
	}
	return AllSeated;
}

// A bool function that checks if a passenger can sit
bool CanPassengerSit(Person _Person)
{
	return (IsPointEqual(_Person.CurrentPos, _Person.Target) && !_Person.IsDoneWithSeatInterference);
}

// A function to update visuals
void UpdateVisuals(Person** _PassengerLocationMatrix[], Map _PlaneMap, int* ShowRPCCount, int* RPSCount, clock_t* OneSecWatchStart, clock_t* OneSecWatchEnd)
{
	PrintField(_PassengerLocationMatrix, _PlaneMap);
	printf("RPS: %d", *ShowRPCCount);

	(*RPSCount)++;

	*OneSecWatchEnd = clock();

	if (TimeDifference(*OneSecWatchStart, *OneSecWatchEnd) >= 1000)
	{
		(*OneSecWatchStart) = clock();
		*ShowRPCCount = *RPSCount;
		*RPSCount = 0;
	}
}

// A function to move a passenger
void PerformPassengerMove(Person* _Passenger, Person** _PassengerLocationMatrix[], Map _PlaneMap, SimulationConfig _BaseRules)
{
	if (!ReduceDelayAction(_Passenger, _PlaneMap))
	{
		if (_Passenger->IsDoneWithSeatInterference)
		{
			SetPassengerPosition(_Passenger, _PassengerLocationMatrix, _PlaneMap, _Passenger->Target);

			_Passenger->IsDoneWithSeatInterference = false;
			_Passenger->IsSeated = true;
		}
		else
		{
			AttemptToMovePassenger(_Passenger, _PassengerLocationMatrix, _PlaneMap, _BaseRules);
		}
	}
}

// A function to attempts to move a passenger one step
void AttemptToMovePassenger(Person* _Passenger, Person** _PassengerLocationMatrix[], Map _PlaneMap, SimulationConfig _BaseRules)
{
	for (int j = 0; j < _Passenger->WalkingSpeed; j++)
	{
		// If passenger found their seat, break
		if (!IsPointEqual(_Passenger->CurrentPos, _Passenger->Target))
		{
			// If passenger is obstructed, no need to keep trying to move
			if (!AttemptMove(_PassengerLocationMatrix, _Passenger, _PlaneMap, _BaseRules))
				break;
		}
		else
			break;
	}
}

// A function to teleport a passenger to a point
void SetPassengerPosition(Person* _Passenger, Person** _PassengerLocationMatrix[], Map _PlaneMap, Point _TargetPoint)
{
	_PassengerLocationMatrix[_TargetPoint.Y][_TargetPoint.X] = _Passenger;

	if (_PassengerLocationMatrix[_Passenger->CurrentPos.Y][_Passenger->CurrentPos.X] == _Passenger)
		_PassengerLocationMatrix[_Passenger->CurrentPos.Y][_Passenger->CurrentPos.X] = NULL;

	_Passenger->CurrentPos = _TargetPoint;
}

// A function that prints visuals
void PrintField(Person** _PassengerLocationMatrix[], Map _PlaneMap)
{
	system("cls");

	for (int y = 0; y < _PlaneMap.Height; y++)
	{
		printf("| ");
		for (int x = 0; x < _PlaneMap.Width; x++)
		{
			if (_PassengerLocationMatrix[y][x] != NULL)
				printf("%-*c", _PlaneMap.LongestNumber, _PassengerLocationMatrix[y][x]->PersonCharacter);
			else
			{
				Location tmpLocation = *GetMapLocation(&_PlaneMap, x, y);
				switch (tmpLocation.BoardingGroup) {
				case BoardingGroup_Door:      printf("%-*c", _PlaneMap.LongestNumber, 'D'); break;
				case BoardingGroup_Walkway:   printf("%-*c", _PlaneMap.LongestNumber, '|'); break;
				case BoardingGroup_Padding:   printf("%-*s", _PlaneMap.LongestNumber, ""); break;
				case BoardingGroup_Undefined: printf("%-*c", _PlaneMap.LongestNumber, 'U'); break;
				default:
					//printf("%-*d", _PlaneMap.LongestNumber, tmpLocation.BoardingGroup);
					printf("%-*s", _PlaneMap.LongestNumber, ".");
					break;
				}
			}
		}
		printf("|\n");
	}

	// Debug
	Sleep(10);
}

// A bool function that checks if there's a passenger on the point a passenger is trying to move to
bool AttemptMove(Person** _PassengerLocationMatrix[], Person* _Passenger, Map _PlaneMap, SimulationConfig _BaseRules)
{
	PerformPassengersNextMove(_Passenger);

	if (IsAnyOnPoint(_PassengerLocationMatrix, _Passenger->NextMove.X, _Passenger->NextMove.Y))
	{
		Person* OtherPerson = _PassengerLocationMatrix[_Passenger->NextMove.Y][_Passenger->NextMove.X];
		PerformPassengersNextMove(OtherPerson);

		// Seat Interference
		if (CheckAndAttemptSeatInterference(_Passenger, OtherPerson, _PassengerLocationMatrix, _PlaneMap, _BaseRules))
			return false;

		// Cross
		CheckAndAttemptCrossing(_Passenger, OtherPerson, _PassengerLocationMatrix, _PlaneMap, _BaseRules);

		return false;
	}

	_Passenger->MovedLastTurn = true;

	SetPassengerPosition(_Passenger, _PassengerLocationMatrix, _PlaneMap, _Passenger->NextMove);

	return true;
}

// A function that checks if a passenger needs to cross
void CheckAndAttemptCrossing(Person* _Person, Person* _OtherPerson, Person** _PassengerLocationMatrix[], Map _PlaneMap, SimulationConfig _BaseRules)
{
	if (IsPointEqual(_OtherPerson->NextMove, _Person->CurrentPos))
	{
		_OtherPerson->CurrentPos = _OtherPerson->NextMove;
		_OtherPerson->CrossDelayCount = _BaseRules.CrossDelay;
		_OtherPerson->MovedLastTurn = true;
		_PassengerLocationMatrix[_OtherPerson->NextMove.Y][_OtherPerson->NextMove.X] = _OtherPerson;

		_Person->CurrentPos = _Person->NextMove;
		_Person->CrossDelayCount = _BaseRules.CrossDelay;
		_Person->MovedLastTurn = true;
		_PassengerLocationMatrix[_Person->NextMove.Y][_Person->NextMove.X] = _Person;
	}
}

// A function that checks for seat interference
bool CheckAndAttemptSeatInterference(Person *_Person, Person *_OtherPerson, Person** _PassengerLocationMatrix[], Map _PlaneMap, SimulationConfig _BaseRules)
{
	if (_Person->Target.Y == _OtherPerson->Target.Y && _Person->CurrentPos.Y == _Person->Target.Y)
	{
		SendRowBack(_PassengerLocationMatrix, _Person, _PlaneMap, _BaseRules);

		return true;
	}
	return false;
}

// A function that performs a passengers next move
void PerformPassengersNextMove(Person *_Passenger)
{
	if (_Passenger->MovedLastTurn)
	{
		_Passenger->NextMove = CalculatePassengersNextPosition(_Passenger->CurrentPos, _Passenger->Target);
		_Passenger->MovedLastTurn = false;
	}
}

// A function that checks for predicted point from a struct
Point CalculatePassengersNextPosition(Point CurrentPoint, Point TargetPoint)
{
	if (CurrentPoint.Y == TargetPoint.Y) 
	{
		if (TargetPoint.X > CurrentPoint.X)
			return SetPoint(CurrentPoint.X + 1, CurrentPoint.Y);
		else
			return SetPoint(CurrentPoint.X - 1, CurrentPoint.Y);
	}
	else
	{
		if (TargetPoint.Y > CurrentPoint.Y)
			return SetPoint(CurrentPoint.X, CurrentPoint.Y + 1);
		else
			return SetPoint(CurrentPoint.X, CurrentPoint.Y - 1);
	}
	return SetPoint(0, 0);
}

// A function that sends row back
void SendRowBack(Person** _PassengerLocationMatrix[], Person* _Passenger, Map _PlaneMap, SimulationConfig _BaseRules)
{
	int CurrentXPosition = _Passenger->Target.X;
	int DistanceToTargetSeat = abs(_Passenger->Target.X - _PlaneMap.Doors[_Passenger->StartingDoorID].X);
	int InnerMostSeat = -1;
	while (CurrentXPosition != _PlaneMap.Doors[_Passenger->StartingDoorID].X)
	{
		if (IsAnyOnPoint(_PassengerLocationMatrix, CurrentXPosition, _Passenger->Target.Y) && CurrentXPosition != _Passenger->CurrentPos.X)
		{
			Person* MomentPerson = _PassengerLocationMatrix[_Passenger->Target.Y][CurrentXPosition];
			if (InnerMostSeat == -1)
				InnerMostSeat = abs(MomentPerson->Target.X - _PlaneMap.Doors[_Passenger->StartingDoorID].X);

			SendPassengerBack(MomentPerson, DistanceToTargetSeat, InnerMostSeat, _BaseRules.SeatInterferenceDelay);
		}

		if (_Passenger->Target.X > _PlaneMap.Doors[_Passenger->StartingDoorID].X)
			CurrentXPosition--;
		else
			CurrentXPosition++;
	}

	SendPassengerBack(_Passenger, DistanceToTargetSeat, InnerMostSeat, _BaseRules.SeatInterferenceDelay);
}

// A function that sends a passenger back
void SendPassengerBack(Person* _Passenger, int _DistanceToTargetSeat, int _InnerMostSeat, int _AdditionalDelay)
{
	_Passenger->IsDoneWithSeatInterference = true;
	_Passenger->SeatInterferenceDelayCount = BackupWaitSteps(_DistanceToTargetSeat, _InnerMostSeat, _AdditionalDelay);
	_Passenger->MovedLastTurn = true;
}

// A bool function that checks if any is on matrix
bool IsAnyOnPoint(Person** _PassengerLocationMatrix[], int X, int Y)
{
	if (_PassengerLocationMatrix[Y][X] != NULL)
		return true;
	return false;
}

// A function for backup wait steps
int BackupWaitSteps(int TargetSeat, int InnerBlockingSeat, int ExtraPenalty)
{
	return TargetSeat + InnerBlockingSeat + 2 + ExtraPenalty;
}

// A bool function to check if a passenger is in delay action
bool ReduceDelayAction(Person* _Person, Map _PlaneMap)
{
	if (_Person->LuggageCount > 0)
	{
		if (IsInFrontOfSeatRow(_Person, _PlaneMap))
		{
			_Person->LuggageCount--;

			return true;
		}
	}

	if (_Person->SeatInterferenceDelayCount > 0)
	{
		_Person->SeatInterferenceDelayCount--;

		return true;
	}

	if (_Person->CrossDelayCount > 0)
	{
		_Person->CrossDelayCount--;

		return true;
	}

	return false;
}

bool IsInFrontOfSeatRow(Person* _Person, Map _PlaneMap)
{
	if (_Person->CurrentPos.Y == _Person->Target.Y && _Person->CurrentPos.X == _PlaneMap.Doors[_Person->StartingDoorID].X)
		return true;
	return false;
}