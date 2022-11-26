extends Spatial

export var speed = 0.0085
var velocity = Vector3()

func _process(delta):
	var velocity = Vector3.ZERO
	if Input.is_action_pressed("ui_right"):
		velocity.x += 1		
	if Input.is_action_pressed("ui_left"):
		velocity.x -= 1
	if Input.is_action_pressed("ui_down"):
		velocity.z += 0.5
	if Input.is_action_pressed("ui_up"):
		velocity.z -= 0.5
#
	if velocity.x > 0:
		$AnimationPlayer.play("right")
	elif velocity.x < 0:
		$AnimationPlayer.play("left")
	elif velocity.z < 0:
		$AnimationPlayer.play("up")
	elif velocity.z > 0:
		$AnimationPlayer.play("down")
	else:
		$AnimationPlayer.stop()

	if velocity.length() > 0:
		#velocity = velocity.normalized() * speed
		velocity = velocity * speed
		translation = translation + velocity
