puts $:.is_a?(Array)
puts $LOAD_PATH.is_a?(Array)
puts $:.object_id == $LOAD_PATH.object_id
