# raise inside ensure replaces the propagating exception
begin
  begin
    raise "original"
  ensure
    raise "from ensure"
  end
rescue => e
  puts e.message
  puts e.class
end

# inner rescue in ensure: original exception continues propagating
begin
  begin
    raise "first"
  ensure
    begin
      raise "ensure error"
    rescue => e
      puts "inner: #{e.message}"
    end
  end
rescue => e
  puts "outer: #{e.message}"
end
