$stdout.sync = true

# exit raises SystemExit which can be rescued

begin
  exit 0
rescue SystemExit => e
  puts e.status
  puts e.success?
end

begin
  exit 7
rescue SystemExit => e
  puts e.status
  puts e.success?
end

begin
  exit true
rescue SystemExit => e
  puts e.status
end

begin
  exit false
rescue SystemExit => e
  puts e.status
end

# SystemExit is an Exception subclass
begin
  exit 1
rescue Exception => e
  puts e.class
  puts e.is_a?(Exception)
end

# abort raises SystemExit with status 1
begin
  abort "error message"
rescue SystemExit => e
  puts e.status
  puts e.success?
end

# exit! bypasses rescue
ran = false
begin
  # Can't test exit! directly without subprocess, so just verify SystemExit#status works
  e = SystemExit.new
  val_obj_status = e.is_a?(Exception)
  puts val_obj_status
rescue => err
  puts "unexpected: #{err.message}"
end
