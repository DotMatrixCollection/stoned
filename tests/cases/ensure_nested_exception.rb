def nested_ensure
  result = []
  begin
    begin
      result << "inner try"
      raise "inner error"
    ensure
      result << "inner ensure"
    end
  rescue => e
    result << "rescued: #{e.message}"
  ensure
    result << "outer ensure"
  end
  result
end
p nested_ensure
