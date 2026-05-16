module OuterAlias
  class Visitor
    def visit_case_node
      "case"
    end

    alias visit_case_match_node visit_case_node
  end
end

puts OuterAlias::Visitor.new.visit_case_match_node
