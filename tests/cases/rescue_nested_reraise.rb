begin
  begin
    raise "boom"
  rescue => e
    begin
      raise e
    rescue
      raise
    end
  end
rescue RuntimeError => outer
  puts outer.message
  raise
end
