class Looper
  def run
    run
  end
end

begin
  Looper.new.run
rescue SystemStackError => e
  puts e.class
  puts e.message
end
