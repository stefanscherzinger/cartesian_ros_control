// -- BEGIN LICENSE BLOCK -----------------------------------------------------
// -- END LICENSE BLOCK -------------------------------------------------------

//-----------------------------------------------------------------------------
/*!\file    trajectory_interface.h
 *
 * \author  Stefan Scherzinger <scherzin@fzi.de>
 * \date    2020/07/15
 *
 */
//-----------------------------------------------------------------------------


#pragma once

#include <functional>
#include <hardware_interface/internal/hardware_resource_manager.h>
#include <cartesian_control_msgs/FollowCartesianTrajectoryGoal.h>
#include <cartesian_control_msgs/FollowCartesianTrajectoryFeedback.h>
#include <control_msgs/FollowJointTrajectoryGoal.h>
#include <control_msgs/FollowJointTrajectoryFeedback.h>
#include <vector>

namespace hardware_interface {

/**
 * @brief TrajectoryType for joint-based trajectories
 */
using JointTrajectory = control_msgs::FollowJointTrajectoryGoal;

/**
 * @brief TrajectoryType for Cartesian trajectories
 */
using CartesianTrajectory = cartesian_control_msgs::FollowCartesianTrajectoryGoal;

/**
 * @brief FeedbackType for joint-based trajectories
 */
using JointTrajectoryFeedback = control_msgs::FollowJointTrajectoryFeedback;

/**
 * @brief FeedbackType for Cartesian trajectories
 */
using CartesianTrajectoryFeedback = cartesian_control_msgs::FollowCartesianTrajectoryFeedback;



/**
 * @brief A class implementing handles for trajectory hardware interfaces
 *
 * This is a special type of interface handle for PassThroughControllers. The
 * handles provide read/write access to trajectory command buffers and
 * read/write access to trajectory feedback buffers.
 *
 * @tparam TrajectoryType The type of trajectory used.
 * @tparam FeedbackType The type of feedback for the trajectory used.
 */
template<class TrajectoryType, class FeedbackType>
class TrajectoryHandle
{
  public:
    TrajectoryHandle() = delete;

    /**
     * @brief A trajectory handle for managing read/write functionality for
     * PassThroughControllers
     *
     * @param cmd The command buffer for read/write operations
     * @param feedback The feedback buffer for read/write operations
     */
    TrajectoryHandle(TrajectoryType* cmd, FeedbackType* feedback)
      : m_cmd(cmd)
      , m_feedback(feedback)
    {
      if (!cmd || !feedback)
      {
        throw HardwareInterfaceException(
          "Cannot create TrajectoryHandle. Make sure to provide both command and feedback buffers during construction");
      }
    };

    /**
     * @brief A trajectory handle for managing read/write functionality for
     * PassThroughControllers
     *
     * Overload for using callbacks to trigger precise start and cancel events
     * in user code. In the context of using PassThroughControllers,
     * implementers of ROS-control HW-interfaces can use this callback
     * mechanism to handle starting and canceling of trajectories on the robot
     * vendor controller.
     *
     * @param cmd The command buffer for read/write operations
     * @param feedback The feedback buffer for read/write operations
     * @param on_new_cmd Callback that is called upon receiving new commands
     * @param on_cancel Callback that is called when current commands are canceled
     */
    TrajectoryHandle(TrajectoryType* cmd,
                     FeedbackType* feedback,
                     std::function<void(const TrajectoryType&)> on_new_cmd,
                     std::function<void()> on_cancel)
      : m_cmd(cmd)
      , m_feedback(feedback)
      , m_cmd_callback(on_new_cmd)
      , m_cancel_callback(on_cancel)
    {
      if (!cmd || !feedback)
      {
        throw HardwareInterfaceException(
          "Cannot create TrajectoryHandle. Make sure to provide both command and feedback buffers during construction");
      }
    };

    ~TrajectoryHandle(){};

    /**
     * @brief Write the command buffer with the content of an new trajectory
     *
     * This will mainly be used by PassThroughControllers to store their new
     * incoming trajectories in the robot hardware interface.
     *
     * @param command The new trajectory
     */
    void setCommand(TrajectoryType command)
    {
      assert(m_cmd);
      *m_cmd = command;

      if (m_cmd_callback)
      {
        m_cmd_callback(*m_cmd);
      }
    }

    /**
     * @brief Read a trajectory from the command buffer
     *
     * This can be used to access content from forwarded trajectories in the
     * robot hardware interface.
     *
     * @return The content of the trajectory command buffer
     */
    TrajectoryType getCommand() const {assert(m_cmd); return *m_cmd;}

    /**
     * @brief Cancel an active command
     */
    void cancelCommand()
    {
      if (m_cancel_callback)
      {
        m_cancel_callback();
      }
    }

    /**
     * @brief Set trajectory feedback for PassThroughControllers
     *
     * This should be used by the robot HW to provide feedback on trajectory
     * execution for the PassThroughControllers
     *
     * @param feedback The feedback content to write to the interface
     */
    void setFeedback(FeedbackType feedback)
    {
      assert(m_feedback);
      *m_feedback = feedback;
    }

    /**
     * @brief Access trajectory feedback
     *
     * This can be used by PassThroughControllers to get trajectory feedback
     * from the hardware interface.
     *
     * @return The most recent feedback on the trajectory execution
     */
    FeedbackType getFeedback() const {assert(m_feedback); return *m_feedback;}

    /**
     * @brief Get the name of this trajectory handle
     *
     * @return The name that is associated with this specific handle
     */
    std::string getName() const noexcept { return "joint_trajectory_handle"; }

  private:
    TrajectoryType* m_cmd;
    FeedbackType* m_feedback;
    std::function<void(const TrajectoryType&)> m_cmd_callback;
    std::function<void()> m_cancel_callback;;
};


using JointTrajectoryHandle = TrajectoryHandle<JointTrajectory, JointTrajectoryFeedback>;
using CartesianTrajectoryHandle = TrajectoryHandle<CartesianTrajectory, CartesianTrajectoryFeedback>;


/**
 * @brief Hardware interface for commanding trajectories
 *
 * This special hardware interface is primarily used by PassThroughControllers,
 * which forward full trajectories to robots for interpolation. In contrast to
 * other hardware interfaces, this interface claims multiple resources and
 * offers write access to full trajectory buffers.
 *
 * @tparam TrajectoryType Distinguish between joint-based and Cartesian trajectories
 * @tparam FeedbackType The type of feedback for the trajectory used.
 */
template <class TrajectoryType, class FeedbackType>
class TrajectoryInterface
  : public hardware_interface::HardwareResourceManager<TrajectoryHandle<TrajectoryType, FeedbackType>,
                                                       hardware_interface::ClaimResources>
{
  public:

    /**
     * @brief Associate resources with this interface
     *
     * Call this during initialization of your controller.
     * \Note: Proper resource handling depends on calling this
     * method \a before acquiring handles to this interface via getHandle().
     *
     * @param resources A list of resource names
     */
    void setResources(std::vector<std::string> resources)
    {
      m_joint_names = resources;
    }

    /**
     * @brief Claim multiple resources when using a single TrajectoryHandle
     *
     * This makes sure that PassThroughControllers claim all associated
     * resources.
     *
     * @param std::string Not used
     */
    void claim(std::string /*resource*/) override
    {
      for (const std::string& joint : m_joint_names)
      {
        hardware_interface::HardwareResourceManager<
          TrajectoryHandle<TrajectoryType, FeedbackType>,
          hardware_interface::ClaimResources>::claim(joint);
      }
    }

  private:
    std::vector<std::string> m_joint_names;
};

/**
 * @brief Hardware interface for commanding (forwarding) joint-based trajectories
 */
using JointTrajectoryInterface = TrajectoryInterface<JointTrajectory, JointTrajectoryFeedback>;

/**
 * @brief Hardware interface for commanding (forwarding) Cartesian trajectories
 */
using CartesianTrajectoryInterface = TrajectoryInterface<CartesianTrajectory, CartesianTrajectoryFeedback>;

}
