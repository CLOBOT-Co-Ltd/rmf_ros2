/*
 * Copyright (C) 2020 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
*/

#include "GoToPlace.hpp"
#include "MoveRobot.hpp"
#include "DoorOpen.hpp"
#include "DoorClose.hpp"
#include "RequestLift.hpp"
#include "DockRobot.hpp"

#include <rmf_traffic/schedule/StubbornNegotiator.hpp>

namespace rmf_fleet_adapter {
namespace phases {

//==============================================================================
auto GoToPlace::Active::observe() const -> const rxcpp::observable<StatusMsg>&
{
  return _status_obs;
}

//==============================================================================
rmf_traffic::Duration GoToPlace::Active::estimate_remaining_time() const
{
  if (_plan)
  {
    if (_plan->get_itinerary().empty())
      return rmf_traffic::Duration(0);

    const auto& traj = _plan->get_itinerary().back().trajectory();
    if (traj.size() == 0)
    {
      // This shouldn't happen
      assert(false);
      return rmf_traffic::Duration(0);
    }

    const auto t = traj.back().time();
    return t - _context->now();
  }

  return rmf_traffic::time::from_seconds(_latest_time_estimate);
}

//==============================================================================
void GoToPlace::Active::emergency_alarm(const bool value)
{
  if (_emergency_active == value)
    return;

  _emergency_active = value;
  if (_emergency_active)
  {
    cancel();
    find_emergency_plan();
  }
  else
  {
    find_plan();
  }
}
//==============================================================================
void GoToPlace::Active::cancel()
{
  if (_subtasks)
    _subtasks->cancel();
}

//==============================================================================
const std::string& GoToPlace::Active::description() const
{
  return _description;
}

#ifdef CLOBER_RMF
//==============================================================================
void GoToPlace::Active::clober_respond(
  const TableViewerPtr& table_viewer,
  const ResponderPtr& responder,
  std::string target_robot_id,
  std::string target_start,
  std::string target_end,
  std::vector<std::string> target_path,
  std::string enemy_robot_id,
  std::string enemy_start,
  std::size_t enemy_startidx,
  std::string enemy_end,
  std::vector<std::string> enemy_path)
{
  // std::cout <<"GoToPlace::Active::clober_respond id : " << target_robot_id <<std::endl;

  // todo
  // 1. excute plan 을 통해 움직이기
  // 2. negotiation 결과 반환 적용

  // if (_subtasks)
  // {
  //   if (dynamic_cast<DockRobot::ActivePhase*>(_subtasks->current_phase().get()))
  //   {
  //     rmf_traffic::schedule::StubbornNegotiator(_context->itinerary())
  //     .respond(table_viewer, responder);
  //     return;
  //   }
  // }


  auto approval_cb = [w = weak_from_this()](
    const rmf_traffic::agv::Plan& plan)
    -> rmf_utils::optional<rmf_traffic::schedule::ItineraryVersion>
    {
      if (auto active = w.lock())
      {
        // std::cout <<"GoToPlace::Active::clober_respond execute_plan 호출" << std::endl;
        active->execute_plan(plan);

        // std::cout <<"GoToPlace::Active::clober_respond execute_plan 결과" << std::endl;

        return active->_context->itinerary().version();
      }
      // std::cout <<"GoToPlace::Active::clober_respond execute_plan 호출하지 않음" << std::endl;

      return rmf_utils::nullopt;
    };

  services::ProgressEvaluator evaluator;
  if (table_viewer->parent_id())
  {
    const auto& s = table_viewer->sequence();
    assert(s.size() >= 2);
    evaluator.compliant_leeway_base *= s[s.size()-2].version + 1;
    evaluator.max_cost_threshold = 90.0 + 30.0*s[s.size()-2].version;
  }

  std::shared_ptr<services::Negotiate> negotiate;
  if (_emergency_active)
  {
    negotiate = services::Negotiate::emergency_pullover(
      _context->planner(), _context->location(), table_viewer, responder,
      std::move(approval_cb), evaluator);
  }
  else
  {
    // std::cout <<"GoToPlace::Active::clober_respond negotiate 생성" << std::endl;

    negotiate = services::Negotiate::path(
      _context->planner(), _context->location(), _goal, table_viewer,
      responder, std::move(approval_cb), evaluator);
  }

    // std::cout <<"GoToPlace::Active::clober_respond negotiate_sub 생성" << std::endl;

    // std::cout <<"GoToPlace::Active::clober_respond test clober_plan 호출" << std::endl;

  std::cout << "conext name: " << _context->name() << std::endl;
  // const auto test_result = _context->planner()->clober_plan(_context->location(), _goal, 
  //  target_robot_id, target_start,
  //           target_end, target_path, enemy_robot_id, enemy_start, enemy_startidx,  enemy_end, enemy_path);

  if(_context->name() == target_robot_id)
  {
    const auto test_result = _context->planner()->clober_plan(_context->location(), _goal, 
      target_robot_id, target_start, target_end, target_path,
      enemy_robot_id, enemy_start, 0,  enemy_end, enemy_path);
    
    execute_plan(*std::move(test_result));
  } else {
    std::string target_robot_id_ = enemy_robot_id;
    std::string target_start_ = enemy_start;
    std::string target_end_ = enemy_end;
    std::vector<std::string> target_path_ = enemy_path; 
    std::string enemy_robot_id_ = target_robot_id;
    std::string enemy_start_ = target_start;
    std::size_t enemy_startidx_ = 0;
    std::string enemy_end_ = target_end;
    std::vector<std::string> enemy_path_ = target_path;

    const auto test_result = _context->planner()->clober_plan(_context->location(), _goal, 
      target_robot_id_, target_start_, target_end_, target_path_,
      enemy_robot_id_, enemy_start_, enemy_startidx_,  enemy_end_, enemy_path_);
    
    execute_plan(*std::move(test_result));
  }

  // std::cout <<"GoToPlace::Active::clober_respond test clober_plan 결과" << std::endl;
    
  // 아래 함수가 plan 호출하는 부분임
  // auto negotiate_sub =
  //   rmf_rxcpp::make_job<services::Negotiate::Result>(negotiate)
  //   .observe_on(rxcpp::identity_same_worker(_context->worker()))
  //   .subscribe(
  //   [w = weak_from_this()](const auto& result)
  //   {
  //     if (auto phase = w.lock())
  //     {
  //       std::cout <<"GoToPlace::Active::clober_respond negotiate_sub 생성 1 (if) " << std::endl;
  //       result.respond();
  //       phase->_negotiate_services.erase(result.service);
  //     }
  //     else
  //     {

  //       std::cout <<"GoToPlace::Active::clober_respond negotiate_sub 생성 (else)" << std::endl;

  //       // We need to make sure we respond in some way so that we don't risk
  //       // making a negotiation hang forever. If this task is dead, then we should
  //       // at least respond by forfeiting.
  //       const auto service = result.service;
  //       const auto responder = service->responder();
  //       responder->forfeit({});
  //     }
  //   });

  // std::cout <<"GoToPlace::Active::clober_respond negotiate_sub 생성 2" << std::endl;

  // using namespace std::chrono_literals;
  // const auto wait_duration = 2s + table_viewer->sequence().back().version * 10s;
  // auto negotiate_timer = _context->node()->try_create_wall_timer(
  //   wait_duration,
  //   [s = negotiate->weak_from_this()]
  //   {
  //     if (const auto service = s.lock())
  //       service->interrupt();
  //   });

  // _negotiate_services[negotiate] =
  //   NegotiateManagers{
  //   std::move(negotiate_sub),
  //   std::move(negotiate_timer)
  // };

  // std::cout <<"GoToPlace::Active::clober_respond negotiate_sub 생성 3" << std::endl;
}
#endif

//==============================================================================
void GoToPlace::Active::respond(
  const TableViewerPtr& table_viewer,
  const ResponderPtr& responder)
{
  if (_subtasks)
  {
    if (dynamic_cast<DockRobot::ActivePhase*>(_subtasks->current_phase().get()))
    {
      rmf_traffic::schedule::StubbornNegotiator(_context->itinerary())
      .respond(table_viewer, responder);
      return;
    }
  }

  #ifdef CLOBER_RMF
  // std::cout << "GoToPlace::Active::respond" << std::endl;
  #endif
  auto approval_cb = [w = weak_from_this()](
    const rmf_traffic::agv::Plan& plan)
    -> rmf_utils::optional<rmf_traffic::schedule::ItineraryVersion>
    {
      if (auto active = w.lock())
      {
        #ifdef CLOBER_RMF
        // std::cout <<"GoToPlace::Active::respond execute_plan 호출" << std::endl;
        #endif
        active->execute_plan(plan);
        #ifdef CLOBER_RMF
        // std::cout <<"GoToPlace::Active::respond execute_plan 결과" << std::endl;
        #endif
        return active->_context->itinerary().version();
      }

      #ifdef CLOBER_RMF
      // std::cout <<"GoToPlace::Active::respond execute_plan 호출하지 않음" << std::endl;
      #endif
      return rmf_utils::nullopt;
    };

  services::ProgressEvaluator evaluator;
  if (table_viewer->parent_id())
  {
    const auto& s = table_viewer->sequence();
    assert(s.size() >= 2);
    evaluator.compliant_leeway_base *= s[s.size()-2].version + 1;
    evaluator.max_cost_threshold = 90.0 + 30.0*s[s.size()-2].version;
  }

  std::shared_ptr<services::Negotiate> negotiate;
  if (_emergency_active)
  {
    negotiate = services::Negotiate::emergency_pullover(
      _context->planner(), _context->location(), table_viewer, responder,
      std::move(approval_cb), evaluator);
  }
  else
  {
    #ifdef CLOBER_RMF
    std::cout <<"GoToPlace::Active::respond negotiate 생성" << std::endl;
    #endif
    negotiate = services::Negotiate::path(
      _context->planner(), _context->location(), _goal, table_viewer,
      responder, std::move(approval_cb), evaluator);
  }

  #ifdef CLOBER_RMF
  std::cout <<"GoToPlace::Active::respond negotiate_sub 생성" << std::endl;
  #endif
  auto negotiate_sub =
    rmf_rxcpp::make_job<services::Negotiate::Result>(negotiate)
    .observe_on(rxcpp::identity_same_worker(_context->worker()))
    .subscribe(
    [w = weak_from_this()](const auto& result)
    {
      if (auto phase = w.lock())
      {
        #ifdef CLOBER_RMF
        std::cout <<"GoToPlace::Active::respond negotiate_sub 생성 1 (if) " << std::endl;
        #endif
        result.respond();
        phase->_negotiate_services.erase(result.service);
      }
      else
      {
        #ifdef CLOBER_RMF
        std::cout <<"GoToPlace::Active::respond negotiate_sub 생성 (else)" << std::endl;
        #endif
        // We need to make sure we respond in some way so that we don't risk
        // making a negotiation hang forever. If this task is dead, then we should
        // at least respond by forfeiting.
        const auto service = result.service;
        const auto responder = service->responder();
        // std::cout << "Check Point" << std::endl;
        responder->forfeit({});
      }
    });

  #ifdef CLOBER_RMF
  // std::cout <<"GoToPlace::Active::respond negotiate_sub 생성 2" << std::endl;
  #endif
  using namespace std::chrono_literals;
  const auto wait_duration = 2s + table_viewer->sequence().back().version * 10s;
  auto negotiate_timer = _context->node()->try_create_wall_timer(
    wait_duration,
    [s = negotiate->weak_from_this()]
    {
      if (const auto service = s.lock())
        service->interrupt();
    });

  _negotiate_services[negotiate] =
    NegotiateManagers{
    std::move(negotiate_sub),
    std::move(negotiate_timer)
  };
  #ifdef CLOBER_RMF
  // std::cout <<"GoToPlace::Active::respond negotiate_sub 생성 3" << std::endl;
  #endif
}

//==============================================================================
GoToPlace::Active::Active(
  agv::RobotContextPtr context,
  rmf_traffic::agv::Plan::Goal goal,
  double original_time_estimate,
  std::optional<rmf_traffic::Duration> tail_period)
: _context(std::move(context)),
  _goal(std::move(goal)),
  _latest_time_estimate(original_time_estimate),
  _tail_period(tail_period)
{
  _description = "Sending [" + _context->requester_id() + "] to ["
    + std::to_string(_goal.waypoint()) + "]";
  _negotiator_license = _context->set_negotiator(this);

  StatusMsg initial_msg;
  initial_msg.status = "Finding a plan for [" + _context->requester_id()
    + "] to go to [" + std::to_string(_goal.waypoint()) + "]";
  initial_msg.start_time = _context->node()->now();
  initial_msg.end_time = initial_msg.start_time;
  _status_publisher.get_subscriber().on_next(initial_msg);
  const auto now = _context->node()->now();
  initial_msg.start_time = now;
  initial_msg.end_time = now + rclcpp::Duration(_latest_time_estimate);

  _status_obs = _status_publisher
    .get_observable()
    .start_with(initial_msg);
}

//==============================================================================
void GoToPlace::Active::find_plan()
{
  #ifdef CLOBER_RMF
  // std::cout <<"start find plan "<<std::endl;
  #endif
  if (_emergency_active)
    return find_emergency_plan();

  StatusMsg msg;
  msg.status = "Finding a plan for [" + _context->requester_id()
    + "] to go to [" + std::to_string(_goal.waypoint()) + "]";
  #ifdef CLOBER_RMF
  // std::cout <<"find plna ~~~~ "<< std::endl;
  // std::cout << msg.status << std::endl; 
  #endif
  msg.start_time = _context->node()->now();
  msg.end_time = msg.start_time;
  _status_publisher.get_subscriber().on_next(msg);

  _pullover_service = nullptr;
  _find_path_service = std::make_shared<services::FindPath>(
    _context->planner(), _context->location(), _goal,
    _context->schedule()->snapshot(), _context->itinerary().id(),
    _context->profile());

  _plan_subscription = rmf_rxcpp::make_job<services::FindPath::Result>(
    _find_path_service)
    .observe_on(rxcpp::identity_same_worker(_context->worker()))
    .subscribe(
    [w = weak_from_this()](
      const services::FindPath::Result& result)
    {
      #ifdef CLOBER_RMF
      // std::cout <<"findpath reuslt~~~~~~~~~~"<<std::endl;
      std::cout << result.success() << std::endl;
      if( result.success()){
        // std::cout <<"result get cost : " << result->get_cost() <<std::endl;
      }
      #endif

      const auto phase = w.lock();
      if (!phase)
        return;

      if (!result)
      {
        // This shouldn't happen, but let's try to handle it gracefully
        phase->_status_publisher.get_subscriber().on_error(
          std::make_exception_ptr(std::runtime_error("Cannot find a plan")));

        // TODO(MXG): Instead of canceling, should we retry later?
        phase->_subtasks->cancel();
        return;
      }

      #ifdef CLOBER_RMF
      // std::cout <<"execute plan 호출"<<std::endl;
      #endif
      phase->execute_plan(*std::move(result));
      #ifdef CLOBER_RMF
      // std::cout <<"execute plan 결과"<<std::endl;
      #endif
      phase->_find_path_service = nullptr;
    });

  // TODO(MXG): Make the timeout configurable
  _find_path_timer = _context->node()->try_create_wall_timer(
    std::chrono::seconds(10),
    [s = std::weak_ptr<services::FindPath>(_find_path_service),
    p = weak_from_this(),
    t = rclcpp::TimerBase::WeakPtr(_find_path_timer)]()
    {
      if (const auto service = s.lock())
        service->interrupt();

      const auto phase = p.lock();
      const auto timer = t.lock();
      if (phase && timer && phase->_find_path_timer == timer)
        phase->_find_path_timer.reset();
    });
}

//==============================================================================
void GoToPlace::Active::find_emergency_plan()
{
  StatusMsg emergency_msg;
  emergency_msg.status = "Planning an emergency pullover for ["
    + _context->requester_id() + "]";
  emergency_msg.start_time = _context->node()->now();
  emergency_msg.end_time = emergency_msg.start_time;
  _status_publisher.get_subscriber().on_next(emergency_msg);

  _find_path_service = nullptr;
  _pullover_service = std::make_shared<services::FindEmergencyPullover>(
    _context->planner(), _context->location(),
    _context->schedule()->snapshot(), _context->itinerary().id(),
    _context->profile());

  _plan_subscription = rmf_rxcpp::make_job<
    services::FindEmergencyPullover::Result>(_pullover_service)
    .observe_on(rxcpp::identity_same_worker(_context->worker()))
    .subscribe(
    [w = weak_from_this()](
      const services::FindEmergencyPullover::Result& result)
    {
      const auto phase = w.lock();
      if (!phase)
        return;

      if (!result)
      {
        // This shouldn't happen, but let's try to handle it gracefully
        phase->_status_publisher.get_subscriber().on_error(
          std::make_exception_ptr(std::runtime_error("Cannot find a plan")));

        // TODO(MXG): Instead of canceling, should we retry later?
        phase->_subtasks->cancel();
        return;
      }

      phase->execute_plan(*std::move(result));
      phase->_performing_emergency_task = true;
      phase->_pullover_service = nullptr;
    });

  _find_pullover_timer = _context->node()->try_create_wall_timer(
    std::chrono::seconds(10),
    [s = _pullover_service->weak_from_this(),
    p = weak_from_this(),
    t = rclcpp::TimerBase::WeakPtr(_find_pullover_timer)]()
    {
      if (const auto service = s.lock())
        service->interrupt();

      const auto phase = p.lock();
      const auto timer = t.lock();
      if (phase && timer && phase->_find_pullover_timer == timer)
        phase->_find_pullover_timer.reset();
    });
}

namespace {
//==============================================================================
class EventPhaseFactory : public rmf_traffic::agv::Graph::Lane::Executor
{
public:

  using Lane = rmf_traffic::agv::Graph::Lane;

  EventPhaseFactory(
    agv::RobotContextPtr context,
    Task::PendingPhases& phases,
    rmf_traffic::Time event_start_time,
    bool& continuous)
  : _context(std::move(context)),
    _phases(phases),
    _event_start_time(event_start_time),
    _continuous(continuous)
  {
    // Do nothing
  }

  void execute(const Dock& dock) final
  {
    assert(!_moving_lift);
    _phases.push_back(
      std::make_unique<phases::DockRobot::PendingPhase>(
        _context, dock.dock_name()));
    _continuous = false;
  }

  void execute(const DoorOpen& open) final
  {
    assert(!_moving_lift);
    const auto node = _context->node();
    _phases.push_back(
      std::make_unique<phases::DoorOpen::PendingPhase>(
        _context,
        open.name(),
        _context->requester_id(),
        _event_start_time + open.duration()));
    _continuous = true;
  }

  void execute(const DoorClose& close) final
  {
    assert(!_moving_lift);

    // TODO(MXG): Account for event duration in this phase
    const auto node = _context->node();
    _phases.push_back(
      std::make_unique<phases::DoorClose::PendingPhase>(
        _context,
        close.name(),
        _context->requester_id()));
    _continuous = true;
  }

  void execute(const LiftSessionBegin& open) final
  {
    assert(!_moving_lift);
    const auto node = _context->node();
    _phases.push_back(
      std::make_unique<phases::RequestLift::PendingPhase>(
        _context,
        open.lift_name(),
        open.floor_name(),
        _event_start_time,
        phases::RequestLift::Located::Outside));

    _continuous = true;
  }

  void execute(const LiftMove& move) final
  {
    // TODO(MXG): We should probably keep track of what lift is being moved to
    // make sure we weren't given a broken nav graph
    _lifting_duration += move.duration();
    _moving_lift = true;

    _continuous = true;
  }

  void execute(const LiftDoorOpen& open) final
  {
    const auto node = _context->node();

    // TODO(MXG): The time calculation here should be considered more carefully.
    _phases.push_back(
      std::make_unique<phases::RequestLift::PendingPhase>(
        _context,
        open.lift_name(),
        open.floor_name(),
        _event_start_time + open.duration() + _lifting_duration,
        phases::RequestLift::Located::Inside));
    _moving_lift = false;

    _continuous = true;
  }

  void execute(const LiftSessionEnd& close) final
  {
    assert(!_moving_lift);
    const auto node = _context->node();
    _phases.push_back(
      std::make_unique<phases::EndLiftSession::Pending>(
        _context,
        close.lift_name(),
        close.floor_name()));

    _continuous = true;
  }

  void execute(const Wait&) final
  {
    // Do nothing
  }

  bool moving_lift() const
  {
    return _moving_lift;
  }

private:
  agv::RobotContextPtr _context;
  Task::PendingPhases& _phases;
  rmf_traffic::Time _event_start_time;
  bool& _continuous;
  bool _moving_lift = false;
  rmf_traffic::Duration _lifting_duration = rmf_traffic::Duration(0);
};

} // anonymous namespace

//==============================================================================
void GoToPlace::Active::execute_plan(rmf_traffic::agv::Plan new_plan)
{
  _plan = std::move(new_plan);

  std::vector<rmf_traffic::agv::Plan::Waypoint> waypoints =
    _plan->get_waypoints();

  #ifdef CLOBER_RMF
  // std::cout <<"excute plan ~~~~~ size : " << waypoints.size() << std::endl;
  #endif

  std::vector<rmf_traffic::agv::Plan::Waypoint> move_through;

  Task::PendingPhases sub_phases;
  while (!waypoints.empty())
  {
    auto it = waypoints.begin();
    bool event_occurred = false;
    for (; it != waypoints.end(); ++it)
    {
      move_through.push_back(*it);

      if (it->event())
      {
        if (move_through.size() > 1)
        {
          sub_phases.push_back(
            std::make_unique<MoveRobot::PendingPhase>(
              _context, move_through, _tail_period));
        }

        move_through.clear();

        bool continuous = true;
        EventPhaseFactory factory(_context, sub_phases, it->time(), continuous);
        it->event()->execute(factory);
        while (factory.moving_lift())
        {
          const auto last_it = it;
          ++it;
          if (!it->event())
          {
            const double dist =
              (it->position().block<2, 1>(0, 0)
              - last_it->position().block<2, 1>(0, 0)).norm();

            if (dist < 0.5)
            {
              // We'll assume that this is just a misalignment in the maps
              continue;
            }

            // TODO(MXG): Figure out how to make this more robust. The current
            // implementation would break if a plan tries to have a robot move
            // itself while the lift is moving between floors.
            _status_publisher.get_subscriber().on_error(
              std::make_exception_ptr(std::runtime_error(
                "We do not support any actions while inside a moving lift")));
            return;
          }

          it->event()->execute(factory);
        }

        if (continuous)
        {
          // Have the next sequence of waypoints begin with the event waypoint
          // of this sequence.
          move_through.push_back(*it);
        }

        // Erase all the used up waypoints.
        waypoints.erase(waypoints.begin(), it+1);
        event_occurred = true;
        break;
      }
    }

    if (move_through.size() > 1)
    {
      /// If we have more than one waypoint to move through, then create a
      /// moving phase.
      sub_phases.push_back(
        std::make_unique<MoveRobot::PendingPhase>(
          _context, move_through, _tail_period));
    }

    if (!event_occurred)
    {
      // If no event occurred on this loop, then we have reached the end of the
      // waypoint sequence, and we should simply clear it out.
      waypoints.clear();
    }
  }

  // TODO: Make distinctions between task and subtasks to avoid passing
  // dummy parameters for subtasks
  rmf_traffic::Time dummy_time;
  rmf_task::agv::State dummy_state{{dummy_time, 0, 0.0}, 0, 1.0};
  _subtasks = Task::make(
    _description,
    std::move(sub_phases),
    _context->worker(),
    dummy_time,
    dummy_state,
    nullptr);

  _status_subscription = _subtasks->observe()
    .observe_on(rxcpp::identity_same_worker(_context->worker()))
    .subscribe(
    [weak = weak_from_this(), r = _context->name()](const StatusMsg& msg)
    {
      if (const auto phase = weak.lock())
        phase->_status_publisher.get_subscriber().on_next(msg);
    },
    [weak = weak_from_this(), r = _context->name()](std::exception_ptr e)
    {
      if (const auto phase = weak.lock())
        phase->_status_publisher.get_subscriber().on_error(e);
    },
    [weak = weak_from_this(), r = _context->name()]()
    {
      if (const auto phase = weak.lock())
      {
        if (!phase->_emergency_active)
          phase->_status_publisher.get_subscriber().on_completed();

        // If an emergency is active, then eventually the alarm should get
        // turned off, which should trigger a non-emergency replanning. That
        // new plan will create a new set of subtasks, and when that new set
        // of subtasks is complete, then we will consider this GoToPlace
        // phase to be complete.
      }
    }
    );
  #ifdef CLOBER_RMF
  // std::cout << "set itinerary" << std::endl;
  // std::vector<rmf_traffic::Route> tmp_route;
  // for(std::size_t i = 0; i < _plan->get_itinerary().size(); i++)
  // {
  //   tmp_route.push_back(_plan->get_itinerary()[i]);
  // }
  
  // std::cout << "itinerary size: " << tmp_route.size() << std::endl;
  // if(tmp_route.size() > 0) {
  //   std::cout << "map name: " << tmp_route[0].map() << std::endl;

  //   std::cout << "Route Trajectory Size: " << tmp_route[0].trajectory().size() << std::endl;
    
  //   for(std::size_t i = 0; i < tmp_route[0].trajectory().size(); i++) {
  //     const rmf_traffic::Trajectory::Waypoint& way = tmp_route[0].trajectory()[i];
  //     std::cout << i << " 번째 waypoint: " << "[" << way.position().x() << ", " << way.position().y() << ", " << way.position().z() << "]" << std::endl;
  //   }
  // }
  #endif
  _subtasks->begin();
  
  _context->itinerary().set(_plan->get_itinerary());
}

//==============================================================================
std::shared_ptr<Task::ActivePhase> GoToPlace::Pending::begin()
{
  #ifdef CLOBER_RMF
  // std::cout <<"GoToPlace::Pending::begin ~~ "<<std::endl;
  #endif
  auto active =
    std::shared_ptr<Active>(
    new Active(_context, _goal, _time_estimate, _tail_period));

  active->find_plan();

  active->_interrupt_subscription = _context->observe_interrupt()
    .observe_on(rxcpp::identity_same_worker(_context->worker()))
    .subscribe(
    [a = active->weak_from_this()](const auto&)
    {
      const auto active = a.lock();
      if (active && !(active->_find_path_service || active->_pullover_service))
      {
        RCLCPP_INFO(
          active->_context->node()->get_logger(),
          "Replanning for [%s] because of an interruption",
          active->_context->requester_id().c_str());
        active->find_plan();
      }
    });

  return active;
}

//==============================================================================
rmf_traffic::Duration GoToPlace::Pending::estimate_phase_duration() const
{
  return rmf_traffic::time::from_seconds(_time_estimate);
}

//==============================================================================
const std::string& GoToPlace::Pending::description() const
{
  return _description;
}

//==============================================================================
GoToPlace::Pending::Pending(
  agv::RobotContextPtr context,
  rmf_traffic::agv::Plan::Goal goal,
  double time_estimate,
  std::optional<rmf_traffic::Duration> tail_period)
: _context(std::move(context)),
  _goal(std::move(goal)),
  _time_estimate(time_estimate),
  _tail_period(tail_period)
{
  _description = "Send robot to [" + std::to_string(_goal.waypoint()) + "]";
}

//==============================================================================
auto GoToPlace::make(
  agv::RobotContextPtr context,
  rmf_traffic::agv::Plan::Start start_estimate,
  rmf_traffic::agv::Plan::Goal goal,
  std::optional<rmf_traffic::Duration> tail_period) -> std::unique_ptr<Pending>
{
  auto estimate_options = context->planner()->get_default_options();
  estimate_options.validator(nullptr);

  #ifdef CLOBER_RMF
  // std::cout <<"GoToPlace::make planner setup 호출 " << std::endl;
  #endif
  
  auto estimate = context->planner()->setup(
    start_estimate, goal, estimate_options);

  if (!estimate.cost_estimate())
  {
    RCLCPP_ERROR(
      context->node()->get_logger(),
      "[GoToPlace] Unable to find any path for robot [%s] to get from "
      "waypoint [%ld] to waypoint [%ld]",
      context->name().c_str(),
      start_estimate.waypoint(),
      goal.waypoint());
    return nullptr;
  }

  const double cost = *estimate.cost_estimate();
  return std::unique_ptr<Pending>(
    new Pending(std::move(context), std::move(goal), cost, tail_period));
}

} // namespace phases
} // namespace rmf_fleet_adapter
