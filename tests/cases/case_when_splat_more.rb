[1, 4, "ok", :other].each do |value|
  result =
    case value
    when 1, 2, 3
      "small"
    when "yes", "ok"
      "word"
    else
      "other"
    end
  p [value, result]
end
