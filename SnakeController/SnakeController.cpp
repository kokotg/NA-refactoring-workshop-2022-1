#include "SnakeController.hpp"

#include <algorithm>
#include <sstream>

#include "EventT.hpp"
#include "IPort.hpp"

namespace Snake
{
ConfigurationError::ConfigurationError()
    : std::logic_error("Bad configuration of Snake::Controller.")
{}

UnexpectedEventException::UnexpectedEventException()
    : std::runtime_error("Unexpected event received!")
{}

Controller::Controller(IPort& p_displayPort, IPort& p_foodPort, IPort& p_scorePort, std::string const& p_config)
    : m_displayPort(p_displayPort),
      m_foodPort(p_foodPort),
      m_scorePort(p_scorePort)
{
    std::istringstream istr(p_config);
    char w, f, s, d;

    int width, height, length;
    int foodX, foodY;
    istr >> w >> width >> height >> f >> foodX >> foodY >> s;

    if (w == 'W' and f == 'F' and s == 'S') {
        m_mapDimension = std::make_pair(width, height);
        m_foodPosition = std::make_pair(foodX, foodY);

        istr >> d;
        switch (d) {
            case 'U':
                m_currentDirection = Direction_UP;
                break;
            case 'D':
                m_currentDirection = Direction_DOWN;
                break;
            case 'L':
                m_currentDirection = Direction_LEFT;
                break;
            case 'R':
                m_currentDirection = Direction_RIGHT;
                break;
            default:
                throw ConfigurationError();
        }
        istr >> length;

        while (length) {
            Segment seg;
            istr >> seg.x >> seg.y;
            seg.ttl = length--;

            m_segments.push_back(seg);
        }
    } else {
        throw ConfigurationError();
    }
}



void Controller::checkLostCondition(Segment& newHead)
{
    if(foodColidedWithSnake(newHead))
    {
        m_scorePort.send(std::make_unique<EventT<LooseInd>>());
        return;
    }

    if (newHead.x < 0 or newHead.y < 0 or
            newHead.x >= m_mapDimension.first or
            newHead.y >= m_mapDimension.second) 
    {
        m_scorePort.send(std::make_unique<EventT<LooseInd>>());
        return;
    } 

    DoSomething1(newHead); 
    DoSomething2(newHead); 
}



void Controller::DoSomething1(Segment& newHead)
{
    if (std::make_pair(newHead.x, newHead.y) == m_foodPosition) 
    {
        m_scorePort.send(std::make_unique<EventT<ScoreInd>>());
        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
        return;
    } 
    for (auto &segment : m_segments) 
    {
        if (not --segment.ttl) 
        {
            DisplayInd l_evt;
            l_evt.x = segment.x;
            l_evt.y = segment.y;
            l_evt.value = Cell_FREE;

            m_displayPort.send(std::make_unique<EventT<DisplayInd>>(l_evt));
        }
    }
}

void Controller::DoSomething2(Segment& newHead)
{
    m_segments.push_front(newHead);
            DisplayInd placeNewHead;
            placeNewHead.x = newHead.x;
            placeNewHead.y = newHead.y;
            placeNewHead.value = Cell_SNAKE;

            m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewHead));

            m_segments.erase(
                std::remove_if(
                    m_segments.begin(),
                    m_segments.end(),
                    [](auto const& segment){ return not (segment.ttl > 0); }),
                m_segments.end());
}

void Controller::InitializeHead(Segment& newHead, Segment const& currentHead)
{
    newHead.x = currentHead.x + ((m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.y = currentHead.y + (not (m_currentDirection & 0b01) ? (m_currentDirection & 0b10) ? 1 : -1 : 0);
    newHead.ttl = currentHead.ttl;
}


void Controller::clearOldFood()
{
    DisplayInd clearOldFood;
    clearOldFood.x = m_foodPosition.first;
    clearOldFood.y = m_foodPosition.second;
    clearOldFood.value = Cell_FREE;
    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(clearOldFood));
}

template <typename T>
void Controller::placeNewFood(T receivedFood)
{
    DisplayInd placeNewFood;
    placeNewFood.x = receivedFood.x;
    placeNewFood.y = receivedFood.y;
    placeNewFood.value = Cell_FOOD;
    m_displayPort.send(std::make_unique<EventT<DisplayInd>>(placeNewFood));
}

template <typename T>
bool Controller::foodColidedWithSnake(T requestedFood)
{
    for (auto const& segment : m_segments) 
    {
        if (segment.x == requestedFood.x and segment.y == requestedFood.y) 
        {
            return true;
        }
    }
    return false;
}

void Controller::receive(std::unique_ptr<Event> e)
{
    try 
    {
        *dynamic_cast<EventT<TimeoutInd> const&>(*e);

        Segment const& currentHead = m_segments.front();

        Segment newHead;
        InitializeHead(newHead, currentHead);

        checkLostCondition(newHead);
    } 
    
    catch (std::bad_cast&) 
    {
        try 
        {
            auto direction = dynamic_cast<EventT<DirectionInd> const&>(*e)->direction;

            if ((m_currentDirection & 0b01) != (direction & 0b01)) 
            {
                m_currentDirection = direction;
            }
        } catch (std::bad_cast&) 
        {
            try 
            {
                auto receivedFood = *dynamic_cast<EventT<FoodInd> const&>(*e);

                if (foodColidedWithSnake(receivedFood))
                {
                    m_foodPort.send(std::make_unique<EventT<FoodReq>>());
                    return;
                }

                clearOldFood();
                placeNewFood(receivedFood);
            } 
            catch (std::bad_cast&) 
            {
                try 
                {
                    auto requestedFood = *dynamic_cast<EventT<FoodResp> const&>(*e);

                    if (foodColidedWithSnake(requestedFood))
                    {
                        m_foodPort.send(std::make_unique<EventT<FoodReq>>());
                        return;
                    }

                    placeNewFood(requestedFood);
                } 
                catch (std::bad_cast&) 
                {
                    throw UnexpectedEventException();
                }
            }
        }
    }
}

} // namespace Snake