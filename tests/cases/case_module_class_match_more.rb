module CaseTag
end

class CaseBase
end

class CaseChild < CaseBase
  include CaseTag
end

[CaseChild.new, CaseBase.new, "ruby"].each do |value|
  result =
    case value
    when CaseTag
      "tag"
    when CaseBase
      "base"
    when String
      "string"
    else
      "other"
    end
  p result
end
