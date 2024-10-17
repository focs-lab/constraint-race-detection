#include <z3++.h>

#include <iostream>
#include <memory>
#include <string>

class IntExpr;
class VarExpr;
class LessThanExpr;
class AndExpr;
class OrExpr;

class ExprVisitor {
   public:
    virtual void visit(const IntExpr& expr) = 0;
    virtual void visit(const VarExpr& expr) = 0;
    virtual void visit(const LessThanExpr& expr) = 0;
    virtual void visit(const AndExpr& expr) = 0;
    virtual void visit(const OrExpr& expr) = 0;
};

class Expr {
   public:
    virtual void accept(ExprVisitor& visitor) const = 0;
    virtual void print() const = 0;
};

class IntExpr : public Expr {
   private:
    int value;

   public:
    IntExpr(int value_) : value(value_) {}

    int getValue() const { return value; }

    void print() const override { std::cout << value; }

    void accept(ExprVisitor& visitor) const override;
};

class VarExpr : public Expr {
   private:
    uint32_t varId;

   public:
    VarExpr(uint32_t varId_) : varId(varId_) {}

    uint32_t getVarId() const { return varId; }

    void print() const override { std::cout << varId; }
    void accept(ExprVisitor& visitor) const override;
};

class BinExpr : public Expr {
   private:
    std::shared_ptr<Expr> lhs;
    std::shared_ptr<Expr> rhs;

   public:
    BinExpr(std::shared_ptr<Expr> lhs_, std::shared_ptr<Expr> rhs_)
        : lhs(lhs_), rhs(rhs_) {}

    const std::shared_ptr<Expr>& getLhs() const { return lhs; }

    const std::shared_ptr<Expr>& getRhs() const { return rhs; }
};

class LessThanExpr : public BinExpr {
   public:
    LessThanExpr(std::shared_ptr<Expr> lhs_, std::shared_ptr<Expr> rhs_)
        : BinExpr(lhs_, rhs_) {}

    void accept(ExprVisitor& visitor) const override;

    void print() const override {
        std::cout << "(";
        getLhs()->print();
        std::cout << " < ";
        getRhs()->print();
        std::cout << ")";
    }
};

class AndExpr : public BinExpr {
   public:
    AndExpr(std::shared_ptr<Expr> lhs_, std::shared_ptr<Expr> rhs_)
        : BinExpr(lhs_, rhs_) {}

    void accept(ExprVisitor& visitor) const override;

    void print() const override {
        std::cout << "(";
        getLhs()->print();
        std::cout << " && ";
        getRhs()->print();
        std::cout << ")";
    }
};

class OrExpr : public BinExpr {
   public:
    OrExpr(std::shared_ptr<Expr> lhs_, std::shared_ptr<Expr> rhs_)
        : BinExpr(lhs_, rhs_) {}

    void accept(ExprVisitor& visitor) const override;

    void print() const override {
        std::cout << "(";
        getLhs()->print();
        std::cout << " || ";
        getRhs()->print();
        std::cout << ")";
    }
};

void IntExpr::accept(ExprVisitor& visitor) const { visitor.visit(*this); }
void VarExpr::accept(ExprVisitor& visitor) const { visitor.visit(*this); }
void LessThanExpr::accept(ExprVisitor& visitor) const { visitor.visit(*this); }
void AndExpr::accept(ExprVisitor& visitor) const { visitor.visit(*this); }
void OrExpr::accept(ExprVisitor& visitor) const { visitor.visit(*this); }

class ExprBuilder {
   public:
    static std::shared_ptr<Expr> intVal(int value) {
        return std::make_shared<IntExpr>(value);
    }

    static std::shared_ptr<Expr> var(uint32_t varId) {
        return std::make_shared<VarExpr>(varId);
    }

    static std::shared_ptr<Expr> lessThan(const std::shared_ptr<Expr>& left,
                                          const std::shared_ptr<Expr>& right) {
        if (left == nullptr || right == nullptr) {
            std::cout << "Invalid less than expr" << std::endl;
            return nullptr;
        }
        return std::make_shared<LessThanExpr>(left, right);
    }

    static std::shared_ptr<Expr> andExpr(const std::shared_ptr<Expr>& left,
                                         const std::shared_ptr<Expr>& right) {
        if (left == nullptr || right == nullptr) {
            std::cout << "Invalid and expr" << std::endl;
            return nullptr;
        }
        return std::make_shared<AndExpr>(left, right);
    }

    static std::shared_ptr<Expr> orExpr(const std::shared_ptr<Expr>& left,
                                        const std::shared_ptr<Expr>& right) {
        if (left == nullptr || right == nullptr) {
            std::cout << "Invalid or expr" << std::endl;
            return nullptr;
        }
        return std::make_shared<OrExpr>(left, right);
    }
};

class Z3ExprVisitor : public ExprVisitor {
   private:
    z3::context& ctx;
    z3::expr result;
    z3::expr_vector& varMap;
    int count;

   public:
    Z3ExprVisitor(z3::context& ctx_, z3::expr_vector& varMap_)
        : ctx(ctx_), result(ctx_), varMap(varMap_) {
        count = 0;
    }

    void visit(const IntExpr& expr) override {
        result = ctx.int_val(expr.getValue());
    }

    void visit(const VarExpr& expr) override {
        if (expr.getVarId() <= 0) {
            std::cout << "Invalid var id: " << expr.getVarId() << std::endl;
            return;
        }
        result = varMap[expr.getVarId() - 1];
    }

    void visit(const LessThanExpr& expr) override {
        expr.getLhs()->accept(*this);
        z3::expr lhs = result;
        expr.getRhs()->accept(*this);
        z3::expr rhs = result;
        result = lhs < rhs;
    }

    void visit(const AndExpr& expr) override {
        expr.getLhs()->accept(*this);
        z3::expr lhs = result;
        expr.getRhs()->accept(*this);
        z3::expr rhs = result;
        result = lhs && rhs;
    }

    void visit(const OrExpr& expr) override {
        expr.getLhs()->accept(*this);
        z3::expr lhs = result;
        expr.getRhs()->accept(*this);
        z3::expr rhs = result;
        result = lhs || rhs;
    }

    z3::expr getResult() const { return result; }
};

class ConstraintCounter : public ExprVisitor {
   private:
    int expr_count;
    int constraint_count;

   public:
    ConstraintCounter() : expr_count(0), constraint_count(0) {}

    void visit(const IntExpr& expr) override { return; }

    void visit(const VarExpr& expr) override { expr_count++; }

    void visit(const LessThanExpr& expr) override {
        expr.getLhs()->accept(*this);
        expr.getRhs()->accept(*this);
        constraint_count++;
    }

    void visit(const AndExpr& expr) override {
        expr.getLhs()->accept(*this);
        expr.getRhs()->accept(*this);
        constraint_count++;
    }

    void visit(const OrExpr& expr) override {
        expr.getLhs()->accept(*this);
        expr.getRhs()->accept(*this);
        constraint_count++;
    }

    int getCount() const { return constraint_count; }
};

class PrintVisitor : public ExprVisitor {
   public:
    void visit(const IntExpr& expr) override { expr.print(); }

    void visit(const VarExpr& expr) override { expr.print(); }

    void visit(const LessThanExpr& expr) override { expr.print(); }

    void visit(const AndExpr& expr) override { expr.print(); }

    void visit(const OrExpr& expr) override { expr.print(); }
};
